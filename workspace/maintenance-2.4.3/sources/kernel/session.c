#include "k.h"
#include "sha256.h"
#define ROUNDS 120000u
// Redundant records in reserved sectors 13/14. Trailer SHA detects torn/corrupt
// records; it is NOT an integrity guarantee against an offline disk editor.
typedef struct PACK {
    u8 magic[8];
    u32 version, rounds;
    char name[32];
    u8 salt[16], verifier[32], padding[384], digest[32];
} Account;
_Static_assert(sizeof(Account) == 512, "account sector");
static Account saved, draft;
static PBKDF job;
static u8 pending_salt[16];
static int salt_ready;
static int state = SESSION_BROKEN, busy, attempts;
static u64 retry_at;
static int same(const u8 *a, const u8 *b, int n)
{
    u32 d = 0;
    for (int i = 0; i < n; i++)d |= a[i] ^ b[i];
    return !d;
}
static int zero(const u8 *p, int n)
{
    u32 v = 0;
    while (n--)v |= *p++;
    return !v;
}
static int field(const char *s, int max)
{
    for (int i = 0; i < max; i++) {
        if (!s[i])return i;
        if ((u8)s[i] < 32 || (u8)s[i] > 126)return -1;
    }
    return -1;
}
static int valid(const Account *a)
{
    u8 h[32];
    sha_bytes(a, 480, h);
    return same(h, a->digest, 32) && !memcmp(a->magic, "TMCUSER2", 8) && a->version == 1 && a->rounds == ROUNDS &&
           field(a->name, 32) > 0;
}
void session_init(void)
{
    Account a, b;
    int x = account_sector(0, &a, 0), y = account_sector(1, &b, 0);
    if (!x && valid(&a)) {
        saved = a;
        state = SESSION_LOCKED;
    } else if (!y && valid(&b)) {
        saved = b;
        state = SESSION_LOCKED;
    } else if ((x == 0 && !zero((u8 *)&a, 512)) || (y == 0 && !zero((u8 *)&b, 512)))
        // A sector that read successfully holds non-zero garbage: fail closed.
        state = SESSION_BROKEN;
    else
        // Blank sectors or unreadable storage (no disk yet): enrollment may
        // proceed; the commit verifies the bytes it wrote before opening.
        state = SESSION_SETUP;
    // Only a genuinely blank account needs entropy. Existing accounts always use
    // their stored salt; invalid records may not re-enter enrollment.
    if (state == SESSION_SETUP) salt_ready = enrollment_salt(pending_salt);
    erase(boot.entropy, sizeof boot.entropy);
    erase(&a, sizeof a);
    erase(&b, sizeof b);
    logstr(state == SESSION_SETUP ? "[session] enrollment required\n" : state == SESSION_LOCKED ?
           "[session] account locked\n" : "[session] unreadable account; fail closed\n");
}
int session_open(void)
{
    return state == SESSION_OPEN;
}
void session_status(SessionInfo *q)
{
    *q = (SessionInfo) {
        .state = state, .working = busy, .needs_seed = state == SESSION_SETUP &&
                                   !salt_ready, .wait_seconds = ticks < retry_at ? (retry_at - ticks + 99) / 100 : 0,
                                                .fraction = busy ? job.done * 100 / ROUNDS : 0
    };
    strset(q->name, state == SESSION_SETUP ? "Utente" : saved.name, sizeof q->name);
}
int session_setup_code(const char text[33])
{
    if (state != SESSION_SETUP || busy || salt_ready || !decode_salt(text, pending_salt))return -1;
    salt_ready = 1;
    logstr("[salt] host setup code accepted\n");
    return 0; // This does not create an account or open a session.
}
// User-confirmed recovery from SESSION_BROKEN: zero both account sectors and
// verify they read back blank, then re-open enrollment. Files are untouched.
static int session_reset(void)
{
    if (state != SESSION_BROKEN || busy)return -1;
    u8 z[512] = {0};
    Account a, b;
    if (account_sector(0, z, 1) || account_sector(1, z, 1))return -1;
    if (account_sector(0, &a, 0) || account_sector(1, &b, 0) ||
            !zero((u8 *)&a, 512) || !zero((u8 *)&b, 512))return -1;
    erase(&a, sizeof a);
    erase(&b, sizeof b);
    state = SESSION_SETUP;
    salt_ready = enrollment_salt(pending_salt);
    logstr("[session] account reset; enrollment re-opened\n");
    return 0;
}
int session_begin(const AuthRequest *q)
{
    if (q->op == 1)return session_reset();
    if (busy || ticks < retry_at || (state != SESSION_SETUP && state != SESSION_LOCKED))return -1;
    int n = field(q->password, sizeof q->password);
    if (n < 1)return -1;
    if (state == SESSION_SETUP) {
        if (n < 8 || field(q->name, 32) < 1)return -1;
        memset(&draft, 0, sizeof draft);
        memcpy(draft.magic, "TMCUSER2", 8);
        draft.version = 1;
        draft.rounds = ROUNDS;
        strset(draft.name, q->name, sizeof draft.name);
        if (!salt_ready)return -3;
        memcpy(draft.salt, pending_salt, 16);
    } else draft = saved;
    pbk_begin(&job, (const u8 *)q->password, n, draft.salt);
    busy = 1;
    return 0;
}
int session_work(void)
{
    if (!busy)return -1;
    pbk_run(&job, MIN(512u, ROUNDS - job.done));
    if (job.done < ROUNDS)return 0;
    int ok = 0, io = 0;
    if (state == SESSION_SETUP) {
        memcpy(draft.verifier, job.sum, 32);
        sha_bytes(&draft, 480, draft.digest);
        Account check; // Commit the backup first, then the primary. Verify the stored bytes before opening.
        int w = account_sector(1, &draft, 1) | account_sector(0, &draft, 1);
        if (w) {
            // The medium refused the write: stay in SETUP so the user can
            // retry; never convert a storage failure into an unreadable account.
            io = 1;
            salt_ready = enrollment_salt(pending_salt);
            logstr("[session] account write failed; still in setup\n");
        } else if (!account_sector(0, &check, 0) && valid(&check) &&
                   same((const u8 *)&check, (const u8 *)&draft, 512)) {
            saved = draft;
            ok = 1;
            erase(pending_salt, sizeof pending_salt);
            salt_ready = 0;
            logstr("[session] verifier enrolled\n");
        } else {
            // The write reported success but read back corrupt: fail closed.
            io = 1;
            state = SESSION_BROKEN;
            erase(pending_salt, sizeof pending_salt);
            salt_ready = 0;
        }
        erase(&check, sizeof check);
    } else ok = same(job.sum, saved.verifier, 32);
    erase(&job, sizeof job);
    erase(&draft, sizeof draft);
    busy = 0;
    if (ok) {
        state = SESSION_OPEN;
        attempts = 0;
        retry_at = 0;
        logstr("[session] authenticated\n");
        return 1;
    }
    if (io)return -3;
    attempts = MIN(attempts + 1, 5);
    retry_at = ticks + (100ull << (attempts - 1));
    logstr("[session] authentication refused\n");
    return -2;
}
int session_lock(void)
{
    if (state != SESSION_OPEN)return -1;
    state = SESSION_LOCKED;
    logstr("[session] locked; clients suspended\n");
    return 0;
}
