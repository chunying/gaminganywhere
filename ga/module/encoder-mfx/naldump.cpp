#include <stdio.h>
#include <string.h>

enum nal_state {
	NS_IDLE = 0,
	NS_FOUND_0,
	NS_FOUND_00,
	NS_FOUND_000,
	NS_FOUND_001,
	NS_FOUND_0001
};

static long long nal_type[32];
static long long nal_count = 0;
static long long nal_totalsize = 0;
static long long nal_size_max = -1;
static long long nal_size_min = 99999999;

void
output_nal(int type, long long offset, long long size) {
	//
	nal_count++;
	nal_totalsize += size;
	printf("nal[%02d]: @ %-12lld size=%lld\n",
		type, offset, size);
	//
	if(size < nal_size_min)
		nal_size_min = size;
	if(size > nal_size_max)
		nal_size_max = size;
	nal_type[type]++;
	return;
}

void
output_statistic() {
	int i, j;
	//
	printf("# total %lld nal(s) found. max=%lld, min=%lld, avg=%lld\n",
		nal_count, nal_size_max, nal_size_min,
		nal_totalsize / nal_count);
	printf(
"# nal  count    %%    nal  count    %%    nal  count    %%    nal  count    %%\n");
	printf(
"# ---- -------- ---- ---- -------- ---- ---- -------- ---- ---- -------- ----\n"); 
	for(i = 0; i < 8; i++) {
		printf("# ");
		for(j = 0; j < 4; j++) {
			int idx = i + j*8;
			printf("%02d   %-8lld %-4.1f ",
				idx, nal_type[idx],
				100.0 * nal_type[idx] / nal_count);
		}
		printf("\n");
	}
	return;
}

int
main(int argc, char *argv[]) {
	FILE *fp;
	char buf[2048];
	int rlen;
	int last_naltype;
	long long last_naloff = 0;
	long long processed = 0;
	enum nal_state state = NS_IDLE;
	//
	if(argc < 2) {
		fprintf(stderr, "usage: %s input.264\n", argv[0]);
		return -1;
	}
	if((fp = fopen(argv[1], "rb")) == NULL) {
		fprintf(stderr, "open %s failed.\n", argv[1]);
		return -1;
	}
	//
	state = NS_IDLE;
	last_naltype = -1;
	last_naloff = 0;
	processed = 0LL;
	while((rlen = fread(buf, sizeof(char), sizeof(buf), fp)) > 0) {
		char *ptr;
		for(ptr = buf; ptr < buf+rlen; ptr++, processed++) {
			switch(state) {
			case NS_IDLE:
				if(*ptr == 0)
					state = NS_FOUND_0;
				break;
			case NS_FOUND_0:
				if(*ptr == 0)
					state = NS_FOUND_00;
				else
					state = NS_IDLE;
				break;
			case NS_FOUND_00:
				if(*ptr == 0)
					state = NS_FOUND_000;
				else if(*ptr == 1)
					state = NS_FOUND_001;
				else
					state = NS_IDLE;
				break;
			case NS_FOUND_000:
				if(*ptr == 1)
					state = NS_FOUND_0001;
				else
					state = NS_IDLE;
				break;
			case NS_FOUND_001:
				if(last_naltype != -1) {
					output_nal(last_naltype, last_naloff, processed-last_naloff-3);
				}
				last_naltype = *ptr & 0x1f;
				last_naloff = processed-3;
				state = NS_IDLE;
				break;
			case NS_FOUND_0001:
				if(last_naltype != -1) {
					output_nal(last_naltype, last_naloff, processed-last_naloff-4);
				}
				last_naltype = *ptr & 0x1f;
				last_naloff = processed-4;
				state = NS_IDLE;
				break;
			}
		}
	}
	fclose(fp);
	// output last
	if(last_naltype != -1) {
		output_nal(last_naltype, last_naloff, processed-last_naloff);
	}
	//
	output_statistic();
	return 0;
}
