#ifndef __BTC_MRE_H__
#define __BTC_MRE_H__

#ifdef __cplusplus
extern "C" {
#endif

// comment to compile for MAINNET
//#define TRY_TESTNET 1

enum btc_wallet_type {
	TYPE_MNEMONIC,
	TYPE_ENTROPY,
	TYPE_SEED,
	TYPE_ROOT_XPRIV,
	TYPE_XPRIV,
	TYPE_PK,
	TYPE_WIF,
	TYPE_MAX_ENUM
};

enum btc_wallet_pub_type {
	TYPE_LEGACY,
	TYPE_SEGWIT,
	TYPE_NESTED_SEGWIT,
};

bool sign_psbt_from_pk(uint8_t* psbt_data, size_t psbt_len, void** signed_psbt_data, size_t* signed_psbt_len);
bool sign_psbt_from_hd(uint8_t* psbt_data, size_t psbt_len, void** signed_psbt_data, size_t* signed_psbt_len);

bool is_wif_valid(char* wif);
int get_pub_type();
void set_pub_type(int pub_type);
char* get_wif();
void set_wif(char* wif);
char* get_mnemo();
void set_mnemo(char* mnemo);
char* get_passphrase();
void set_passphrase(char* passphrase);
char* get_derivationpath();
char* get_acc_ext_derivationpath();
void set_derivationpath(char* dp, char* acc_ext_dp);
bool get_hardened();
void set_hardened(bool hardened);
int get_type();
void set_type(int type);
void derive_xpriv();
void create_priv_key();
void cleanup_sensitive_data();

char* get_acc_ext_pub();
void set_acc_ext_pub(char* str);
char* get_recv_pubs(int idx);
void set_recv_pubs(char* str, int idx);

char* get_xpub();
char* get_xpriv();
void set_xpub(char* str);
void set_xpriv(char* str);

char* get_pub();
void set_pub(char* str);
char* get_priv();
void set_priv(char* str);

size_t c_fromBase64Length(const char * array, size_t arraySize, uint8_t flags);
size_t c_fromBase64(const char * encoded, size_t encodedSize, uint8_t * output, size_t outputSize, uint8_t flags);

#ifdef __cplusplus
}
#endif


#endif //__BTC_MRE_H__