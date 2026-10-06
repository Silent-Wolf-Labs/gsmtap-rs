/* Direct observations from the pinned local libosmocore APIs. Pattern
 * construction below supplies inputs only; C calls produce all outputs. */
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <osmocom/core/bits.h>

static unsigned int observations;
static const unsigned int offsets[] = {0,1,2,3,4,5,6,7,8,9,15,16};
static const unsigned int counts[] = {1,7,8,9,15,16,17,31,32,33};
static const int modes[] = {0,1,2,-1};

static size_t packed_size(unsigned int n) { return n / 8 + (n % 8 != 0); }
static void hex(const uint8_t *p, size_t n)
{
	for (size_t i = 0; i < n; i++) printf("%02x", p[i]);
}
static void observe(const char *name, int pack, unsigned int out,
	unsigned int in, unsigned int n, int mode, const uint8_t *src,
	size_t src_len, size_t dst_len, unsigned int init)
{
	uint8_t *input = malloc(src_len), *dst = malloc(dst_len), *before = malloc(dst_len);
	if (!input || !dst || !before) { fputs("allocation failed\n", stderr); exit(1); }
	memcpy(input, src, src_len);
	for (size_t i = 0; i < dst_len; i++)
		before[i] = dst[i] = init == 0 ? 0 : init == 1 ? 0xff : (uint8_t)(0xa5 ^ i);
	int rc = pack ? osmo_ubit2pbit_ext(dst,out,input,in,n,mode)
		: osmo_pbit2ubit_ext(dst,out,input,in,n,mode);
	printf("%s{\"case\":\"%s\",\"api\":\"%s\",", observations++ ? ",\n" : "",name,
		pack ? "osmo_ubit2pbit_ext" : "osmo_pbit2ubit_ext");
	printf("\"libosmocore_commit\":\"950430e829a3dc1d162aa241bc0505745c5a7311\",\"c_unsigned_bits\":%zu,",sizeof(unsigned int)*CHAR_BIT);
	printf("\"out_ofs\":%u,\"in_ofs\":%u,\"num_bits\":%u,\"lsb_mode\":%d,\"src_hex\":\"",out,in,n,mode);
	hex(input,src_len);
	printf("\",\"dst_len\":%zu,\"dst_before_hex\":\"",dst_len); hex(before,dst_len);
	printf("\",\"return_code\":%d,\"dst_after_hex\":\"",rc); hex(dst,dst_len); printf("\"}");
	free(input); free(dst); free(before);
}
/* Logical patterns are encoded into source storage, never used as outputs. */
static void pattern_source(uint8_t *src, int pack, unsigned int in,
	unsigned int n, int mode, unsigned int pattern, unsigned int tail)
{
	memset(src, tail ? 0xff : 0, 64);
	for (unsigned int i = 0; i < n; i++) {
		int bit = pattern == 1 || (pattern == 2 && i % 2 == 0) || (pattern >= 3 && i == pattern-3);
		unsigned int pos = in+i;
		if (pack) src[pos] = bit;
		else {
			unsigned int bn = mode ? pos%8 : 7-pos%8;
			if (bit) src[pos/8] |= 1u << bn;
			else src[pos/8] &= ~(1u << bn);
		}
	}
}
static void patterned(const char *name, int pack, unsigned int out, unsigned int in,
	unsigned int n, int mode, unsigned int pattern, unsigned int extra,
	unsigned int init, unsigned int tail)
{
	uint8_t src[64];
	pattern_source(src,pack,in,n,mode,pattern,tail);
	size_t used = pack ? in+n : packed_size(in+n);
	size_t dst_len = (pack ? packed_size(out+n) : out+n) + extra*3;
	observe(name,pack,out,in,n,mode,src,used+3,dst_len,init);
}
int main(void)
{
	/* Fixture ABI is explicit, including the zero-bit unsigned underflow. */
	if (CHAR_BIT != 8 || sizeof(unsigned int)*CHAR_BIT != 32 || INT_MAX != 2147483647) {
		fputs("unsupported C integer ABI\n",stderr); return 1;
	}
	char name[160];
	puts("[");
	for (unsigned int byte=0; byte<256; byte++)
	for (unsigned int ofs=0; ofs<8; ofs++)
	for (unsigned int n=1; n<=8-ofs; n++)
	for (int pack=0; pack<=1; pack++)
	for (int mode=0; mode<=1; mode++) {
		uint8_t src[11];
		memset(src,0xa5,sizeof(src));
		unsigned int out = pack ? ofs : (byte+n)%8, in = pack ? 3+ofs : ofs;
		if (pack) for (unsigned int i=0;i<8;i++) src[3+i]=(byte >> (mode ? i : 7-i))&1;
		else src[0]=byte;
		unsigned int extra=(byte+ofs+n+pack+mode)%2, init=(byte+ofs+n+pack+mode)%3;
		snprintf(name,sizeof(name),"exhaustive_b%02x_s%u_n%u_p%d_m%d",byte,ofs,n,pack,mode);
		observe(name,pack,out,in,n,mode,src,pack ? 11 : 1,
			(pack ? packed_size(out+n) : out+n)+extra*3,init);
	}
	for (unsigned int oi=0;oi<12;oi++)
	for (unsigned int pairing=0;pairing<2;pairing++)
	for (unsigned int ni=0;ni<10;ni++)
	for (unsigned int pat=0;pat<3+counts[ni];pat++)
	for (int pack=0;pack<=1;pack++)
	for (int mode=0;mode<=1;mode++) {
		unsigned int out=offsets[oi], in=offsets[(oi+pairing)%12], n=counts[ni];
		unsigned int seed=oi+pairing+ni+pat+pack+mode;
		snprintf(name,sizeof(name),"boundary_o%u_i%u_n%u_t%u_p%d_m%d",out,in,n,pat,pack,mode);
		patterned(name,pack,out,in,n,mode,pat,seed%2,seed%3,0);
	}
	for (unsigned int oi=0;oi<12;oi++)
	for (unsigned int ii=0;ii<12;ii++)
	for (unsigned int ni=0;ni<10;ni++)
	for (int pack=0;pack<=1;pack++)
	for (int mode=0;mode<=1;mode++) {
		unsigned int seed=oi+ii+ni+pack+mode;
		snprintf(name,sizeof(name),"offsets_o%u_i%u_n%u_p%d_m%d",offsets[oi],offsets[ii],counts[ni],pack,mode);
		patterned(name,pack,offsets[oi],offsets[ii],counts[ni],mode,2,seed%2,seed%3,0);
	}
	const uint8_t truthy[]={2,0x80,0xff};
	const unsigned int small_counts[]={1,9,17};
	for (unsigned int v=0;v<3;v++)
	for (unsigned int oi=0;oi<12;oi++)
	for (unsigned int mi=0;mi<4;mi++)
	for (unsigned int ni=0;ni<3;ni++)
	for (unsigned int init=0;init<3;init++) {
		uint8_t src[23]; memset(src,0x5a,sizeof(src));
		unsigned int n=small_counts[ni];
		for(unsigned int i=0;i<n;i++) src[3+i]=i%2 ? 0 : truthy[v];
		snprintf(name,sizeof(name),"truthy_v%02x_o%u_n%u_m%d_d%u",truthy[v],offsets[oi],n,modes[mi],init);
		observe(name,1,offsets[oi],3,n,modes[mi],src,n+6,packed_size(offsets[oi]+n)+3,init);
	}
	for (int pack=0;pack<=1;pack++)
	for (unsigned int mi=0;mi<4;mi++)
	for (unsigned int oi=0;oi<12;oi++)
	for (unsigned int tail=0;tail<2;tail++) {
		int mode=modes[mi];
		snprintf(name,sizeof(name),"neighbors_o%u_p%d_m%d_t%u",offsets[oi],pack,mode,tail);
		patterned(name,pack,offsets[oi],9,17,mode,2,1,2,tail);
	}
	const unsigned int zero_out[]={0,1,7,8,9,16,65}, zero_in[]={0,9,65};
	for (int pack=0;pack<=1;pack++)
	for (unsigned int mi=0;mi<4;mi++)
	for (unsigned int oi=0;oi<7;oi++)
	for (unsigned int ii=0;ii<3;ii++) {
		uint8_t src=0xff;
		snprintf(name,sizeof(name),"zero_o%u_i%u_p%d_m%d",zero_out[oi],zero_in[ii],pack,modes[mi]);
		observe(name,pack,zero_out[oi],zero_in[ii],0,modes[mi],&src,1,1,(oi+ii+mi+pack)%3);
	}
	puts("\n]");
	if (observations != 63384) { fputs("case count changed\n",stderr); return 1; }
	return ferror(stdout) ? 1 : 0;
}
