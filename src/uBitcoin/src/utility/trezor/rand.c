/**
 * Copyright (c) 2013-2014 Tomas Dzetkulic
 * Copyright (c) 2013-2014 Pavol Rusnak
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "rand.h"
#include "sha2.h"
#include "chacha_drbg.h"
#include "mre_menu.h"
#include <string.h>

#include "vmchset.h"
#include "vmsys.h"
#include "vmio.h"

#define malloc vm_malloc
#define free vm_free
#define calloc(x,y) vm_calloc(x*y)
#define realloc vm_realloc

#include "vmche.h"
#include "vmmm.h"

static void drbg_init();

static uint32_t seed = 0;
static uint8_t hash[32];

static void get_space(VMUINT*sys_driver_size, VMUINT*rem_driver_size)
{
	 VMCHAR driver_str[2] = {0};
	 VMWCHAR w_driver_str[4] = {0};
	 VMINT driver = -1;
	 *sys_driver_size = 0;
	 *rem_driver_size = 0;
	 if ((driver = vm_get_system_driver()) >= 0)
	 {
		 sprintf(driver_str, "%c", (VMCHAR)driver);
		 vm_ascii_to_ucs2(w_driver_str, 4, driver_str);
		 *sys_driver_size = vm_get_disk_free_space(w_driver_str);
	 }
 	 if ((driver = vm_get_removable_driver()) >= 0)
	 {   
		 sprintf(driver_str, "%c", (VMCHAR)driver);
		 vm_ascii_to_ucs2(w_driver_str, 4, driver_str);
		 *rem_driver_size = vm_get_disk_free_space(w_driver_str);
	 }
}

static int g_rec_entropy = 0;
void vm_rec_callback(VMINT result)
{
	g_rec_entropy = 1;
}

void sha256_file(const VMWSTR w_path, size_t max_size, size_t skip_offset, uint8_t digest[SHA256_DIGEST_LENGTH]) // skip_offset to skip e.g. headers
{
			VMFILE rec_handle = -1;
			VMUINT rec_file_size = 0;
			void* rec_file_data = NULL;
			size_t nread = 0;

			rec_handle = vm_file_open(w_path, MODE_READ, TRUE);
			if (rec_handle > 0)
			{
				vm_file_getfilesize(rec_handle, &rec_file_size);
				if (rec_file_size > max_size + skip_offset)
				{
					rec_file_size = max_size + skip_offset;
				}
				rec_file_data = vm_malloc(rec_file_size);
				if (rec_file_data && rec_file_size > skip_offset)
				{
					vm_file_read(rec_handle, rec_file_data, rec_file_size, &nread);
					sha256_Raw((uint8_t*)rec_file_data+skip_offset, rec_file_size-skip_offset, digest);
				}

				// read buff
				vm_file_close(rec_handle);
			}

			if (rec_file_data)
			{
				vm_free(rec_file_data);
			}
}


// we use
// vm_che (not sure if hw or sw crypto engine, Mediatek MRE internals are unknown)
//   at least vm_che seems not to be based on rand/srand (cause rand/srand are predictable on emu)
//   but vm_che is not
// rand() (predictable on emulator, on real device no idea how srand is initialized, do not trust anyway) 
//   we do srand with keyboard + vm_che rand key + os tick, but we don't use rand for anything in particular anyway
//   just buffer init, and then we re-populate those buffers
//   so just a fail safe mechanism if we fail populating those
// keyboard inputs
// os ticks
// remaining space on phone
// remaining space on sd
// mic recording hash (not working on emulator)
// pictures hash
// files hash
// imsi
// imei
// operator code
// TODO: maybe add dice rolls? am I being too paranoid? Paranoid is good :)
static void init_ram_seed(){
	extern VMWCHAR w_text_entropy[256];
	VMWCHAR zero_arr[256];
	unsigned char rand_bytes[1024];
	int i=0;
	SHA256_CTX	context;
	
	int rec_res = VM_RECORD_FAILED;
	VMWCHAR rec_full_path[256];
	uint8_t rec_digest[SHA256_DIGEST_LENGTH];
	char drive[256];

	VMINT find_handle = -1;
	VMINT find_next = -1;
	VMWCHAR find_path[256];
	struct vm_fileinfo_t find_info;
	int find_path_len = 0;
	
	int res_memcmp1 = 0;
	int res_memcmp2 = 0;

	VMUINT sys_driver_size = 0;
	VMUINT rem_driver_size = 0;

	VMSTR imsi = NULL;
	VMSTR imei = NULL;
	int res_query_op = -1;

	char operator_code[64];

	VMINT ticks = vm_get_tick_count();
	
	unsigned int dh_key[64/sizeof(unsigned int)];

	vm_stche che_str;

	// vm_che
	vm_che_init(&che_str, VM_CHE_DH);
	vm_che_key_action(&che_str, VM_CHE_GEN_KEY, (VMUINT8*)dh_key, sizeof(dh_key));
	vm_che_deinit(&che_str);	

	memset(rec_full_path, 0, sizeof(rec_full_path));

	sprintf(drive, "%c", mre_get_drv());

	vm_audio_stop_all();
	rec_res = vm_record_start(drive, "\\", "rec_entropy", VM_FORMAT_AMR, rec_full_path, vm_rec_callback);

	set_view(NK_MRE_VIEW_ENTROPY_GEN_WAIT);
	update_gui();

	memset(zero_arr, 0, sizeof(zero_arr));

	// sanity check
	res_memcmp1 = memcmp(zero_arr, w_text_entropy, sizeof(w_text_entropy));
	if (res_memcmp1 == 0)
	{
		// abort
		//printf("exit 2\n");
		vm_exit_app();while(1){};
	}

	// this is just the beginning of the seeding, we do not rely on rand/srand
	// we seed with srand just to initialize random buffers 
	// (they could be initialized to zero as well, so no big deal)
	// we don't really use rand() for anything else
	sha256_Raw((uint8_t*)w_text_entropy, sizeof(w_text_entropy), rec_digest);
	srand(rand() + dh_key[0] + (unsigned int)*(unsigned int**)rec_digest - vm_get_tick_count());

	for (i=0; i<sizeof(rand_bytes); i++)
	{
		rand_bytes[i] = rand() & 0xFF;
	}
	for (i=0; i<sizeof(operator_code); i++)
	{
		operator_code[i] = rand() & 0xFF;
	}
	for (i=0; i<sizeof(rec_digest); i++)
	{
		rec_digest[i] = rand() & 0xFF;
	}
	
	// get remaining space from phone and sd card
	get_space(&sys_driver_size, &rem_driver_size);

	// it takes a few attempts to init on real device
	for (i=0; i<64; i++)
	{
		imsi = vm_get_imsi();
		imei = vm_get_imei();
		res_query_op = vm_query_operator_code(operator_code, sizeof(operator_code));
	}

	sha256_Init(&context);
	sha256_Update(&context, (uint8_t*)dh_key, sizeof(dh_key));
	sha256_Update(&context, (uint8_t*)&sys_driver_size, sizeof(sys_driver_size));
	sha256_Update(&context, (uint8_t*)&rem_driver_size, sizeof(rem_driver_size));
	if (imsi)
	{
		sha256_Update(&context, imsi, strlen(imsi));
	}
	if (imei)
	{
		sha256_Update(&context, imei, strlen(imei));
	}
	sha256_Update(&context, operator_code, sizeof(operator_code)); // even if it fails it still contains a random
	sha256_Update(&context, rand_bytes, sizeof(rand_bytes));
	sha256_Update(&context, (uint8_t*)w_text_entropy, sizeof(w_text_entropy));
	sha256_Update(&context, (uint8_t*)&ticks, sizeof(ticks));	

	// add entropy from random files content (photos then root)
	//sprintf(drive, "%c:\\Photos\\", mre_get_drv());
	//vm_ascii_to_ucs2(find_path, 256, drive);
	//vm_file_mkdir(find_path);

	sprintf(drive, "%c:\\Photos\\*", mre_get_drv());
	vm_ascii_to_ucs2(find_path, 256, drive);
	find_path_len = vm_wstrlen(find_path);
	find_handle = vm_find_first(find_path, &find_info);
	if (find_handle >= 0)
	{
		do {
			if (find_info.size > 0)
			{
				vm_wstrcpy(&find_path[find_path_len-1], find_info.filename);
				sha256_file(find_path, 0x8000, 0, rec_digest);
				sha256_Update(&context, (uint8_t*)rec_digest, sizeof(rec_digest));
			}
			find_next = vm_find_next(find_handle, &find_info);
		} while (find_next >= 0);

		vm_find_close(find_handle);
	}
	sprintf(drive, "%c:\\*", mre_get_drv());
	vm_ascii_to_ucs2(find_path, 256, drive);
	find_path_len = vm_wstrlen(find_path);
	find_handle = vm_find_first(find_path, &find_info);
	if (find_handle >= 0)
	{
		do {
			if (find_info.size > 0)
			{
				vm_wstrcpy(&find_path[find_path_len-1], find_info.filename);
				sha256_file(find_path, 0x8000, 0, rec_digest);
				sha256_Update(&context, (uint8_t*)rec_digest, sizeof(rec_digest));
			}
			find_next = vm_find_next(find_handle, &find_info);
		} while (find_next >= 0);

		vm_find_close(find_handle);
	}

	// add mic rec entropy
	if (rec_res == VM_RECORD_SUCCEED)
	{
		update_gui(); // not needed but use it to let more time pass so we have a longer rec
		rec_res = vm_record_stop();
		if (rec_res == VM_RECORD_SUCCEED)
		{
			// wait for callback
			while (g_rec_entropy == 0) {};

			if (rec_full_path[0] != 0 && rec_full_path[1] != 0)
			{
				sha256_file(rec_full_path, 0x8000, 64, rec_digest);
				vm_file_delete(rec_full_path);
			}
		}
	}
	
	sha256_Update(&context, (uint8_t*)rec_digest, sizeof(rec_digest));

	sha256_Final(&context, hash);

	// wipe entropy once seed is initialized
	memset(w_text_entropy, 0, sizeof(w_text_entropy));
	memset(zero_arr, 0, sizeof(zero_arr));
	memset(rand_bytes, 0, sizeof(rand_bytes));
	memset(&context, 0, sizeof(context));
	memset(rec_full_path, 0, sizeof(rec_full_path));
	memset(rec_digest, 0, sizeof(rec_digest));
	memset(drive, 0, sizeof(drive));
	memset(find_path, 0, sizeof(find_path));
	memset(&find_info, 0, sizeof(find_info));
	memset(operator_code, 0, sizeof(operator_code));
	memset(dh_key, 0, sizeof(dh_key));

	sys_driver_size = 0;
	rem_driver_size = 0;
	imsi = NULL;
	imei = NULL;
	ticks = 0;

	seed++;

	drbg_init();
}

static void random_reseed(const uint32_t value)
{
	seed = value;
}

uint32_t stage_1_random32(void){
	SHA256_CTX	context;
	uint32_t * results = NULL;
	if(seed == 0){
		init_ram_seed();
	}
	sha256_Init(&context);
	sha256_Update(&context, hash, 32);
	sha256_Update(&context, (uint8_t *)&seed, 4);
	sha256_Final(&context, hash);
	results = (uint32_t *)hash;
	seed = results[0];
	return results[1];
}

// #endif /* RAND_PLATFORM_INDEPENDENT */

void stage_1_random_buffer(uint8_t *buf, size_t len)
{
	size_t i = 0;
	uint32_t r = 0;
	for (i = 0; i < len; i++) {
		if (i % 4 == 0) {
			r = stage_1_random32();
		}
		buf[i] = (r >> ((i % 4) * 8)) & 0xFF;
	}
}

void random_buffer(uint8_t *buf, size_t len)
{
	size_t i = 0;
	uint32_t r = 0;
	for (i = 0; i < len; i++) {
		if (i % 4 == 0) {
			r = random32();
		}
		buf[i] = (r >> ((i % 4) * 8)) & 0xFF;
	}
}

uint32_t random_uniform(uint32_t n)
{
	uint32_t x, max = 0xFFFFFFFF - (0xFFFFFFFF % n);
	while ((x = random32()) >= max);
	return x / (max / n);
}

void random_permute(char *str, size_t len)
{
	int i = 0;
	for (i = len - 1; i >= 1; i--) {
		int j = random_uniform(i + 1);
		char t = str[j];
		str[j] = str[i];
		str[i] = t;
	}
}

////////////////////

#define DRBG_RESEED_INTERVAL_CALLS 1000
#define DRBG_TRNG_ENTROPY_LENGTH 50
#define BUFFER_LENGTH 64

static CHACHA_DRBG_CTX drbg_ctx;
static int drbg_initialized = false;

static void drbg_init() {
  uint8_t entropy[DRBG_TRNG_ENTROPY_LENGTH] = {0};
  stage_1_random_buffer(entropy, sizeof(entropy));
  chacha_drbg_init(&drbg_ctx, entropy, sizeof(entropy), NULL, 0);
  memzero(entropy, sizeof(entropy));

  drbg_initialized = true;
}

static void drbg_reseed() {
  uint8_t entropy[DRBG_TRNG_ENTROPY_LENGTH] = {0};
  //ensure(drbg_initialized, NULL);
  if (drbg_initialized == false)
  {
	drbg_init();
  }

  stage_1_random_buffer(entropy, sizeof(entropy));
  chacha_drbg_reseed(&drbg_ctx, entropy, sizeof(entropy), NULL, 0);
  memzero(entropy, sizeof(entropy));
}

static void drbg_generate(uint8_t *buffer, size_t length) {
  if (drbg_initialized == false)
  {
	drbg_init();
  }

  if (drbg_ctx.reseed_counter > DRBG_RESEED_INTERVAL_CALLS) {
    drbg_reseed();
  }
  chacha_drbg_generate(&drbg_ctx, buffer, length);
}

static uint32_t drbg_random8(void) {
  static size_t buffer_index = 0;
  static uint8_t buffer[BUFFER_LENGTH] = {0};
  size_t buffer_index_local = 0;
  uint8_t value = 0;

  if (buffer_index == 0) {
    drbg_generate(buffer, sizeof(buffer));
  }

  buffer_index_local = buffer_index % sizeof(buffer);
  value = buffer[buffer_index_local];
  memzero(&buffer[buffer_index_local], 1);
  buffer_index = (buffer_index_local + 1) % sizeof(buffer);

  return value;
}

uint32_t random32(void) {
  uint32_t value = 0;
  uint8_t* value_char = (uint8_t*)&value;

  value_char[3] = drbg_random8();
  value_char[0] = drbg_random8();
  value_char[2] = drbg_random8();
  value_char[1] = drbg_random8();

  return value;
}

uint32_t random_range_max_excluded(int min, int max_excluded) 
{
   return min + random32() % (max_excluded - min);
}