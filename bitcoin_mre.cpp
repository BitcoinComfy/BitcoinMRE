#include "Bitcoin.h"
#include "PSBT.h"
#include "bitcoin_mre.h"
#include "vmsys.h"
#include "vmio.h"
#include "vmgraph.h"
#include "vmchset.h"
#include "vmstdlib.h"
#include "vmche.h"
#include "vm4res.h"
#include "vmlog.h"

static int g_type = TYPE_MNEMONIC;

static HDPrivateKey *g_root_xprivate_key;
static char* g_mnemo = NULL;
static char* g_passphrase = NULL;
static char* g_derivationpath = NULL;
static char* g_acc_ext_derivationpath = NULL;
static bool g_hardened = false;
static char* g_xpriv = NULL;
static char* g_xpub = NULL;

static PrivateKey *g_private_key;
static char* g_wif = NULL;
static char* g_pub = NULL;
static char* g_priv = NULL;
static int g_pub_type = 0;

extern "C" int get_pub_type()
{
	return g_pub_type;
}

extern "C" void set_pub_type(int pub_type)
{
	g_pub_type = pub_type;
}

extern "C" bool is_wif_valid(char* wif)
{
#ifndef TRY_TESTNET
	PrivateKey* priv = new PrivateKey(wif);
#else
	PrivateKey* priv = new PrivateKey(wif, &Testnet);
#endif
	bool isValid = priv->isValid();
	priv->~PrivateKey();
	if (isValid)
	{
		return true;
	} else {
		return false;
	}
}

extern "C" char* get_wif()
{
	return g_wif;	
}

extern "C" void set_wif(char* wif)
{
	int wif_len = strlen(wif)+1;
	char* old_alloc = g_wif;
	g_wif = (char*)vm_calloc(wif_len);
	strcpy(g_wif, wif);
	if (old_alloc)
	{
		memset(old_alloc, 0, strlen(old_alloc));
		vm_free(old_alloc);
	}
	g_type = TYPE_WIF;
}

extern "C" char* get_mnemo()
{
	return g_mnemo;	
}

extern "C" void set_mnemo(char* mnemo)
{
	int mnemo_len = strlen(mnemo)+1;
	char* old_alloc = g_mnemo;
	g_mnemo = (char*)vm_calloc(mnemo_len);
	strcpy(g_mnemo, mnemo);
	if (old_alloc)
	{
		memset(old_alloc, 0, strlen(old_alloc));
		vm_free(old_alloc);
	}
	g_type = TYPE_MNEMONIC;
}

extern "C" char* get_passphrase()
{
	return g_passphrase;	
}

extern "C" void set_passphrase(char* passphrase)
{
	int passphrase_len = strlen(passphrase)+1;
	char* old_alloc = g_passphrase;
	g_passphrase = (char*)vm_calloc(passphrase_len);
	strcpy(g_passphrase, passphrase);
	if (old_alloc)
	{
		memset(old_alloc, 0, strlen(old_alloc));
		vm_free(old_alloc);
	}	
}

extern "C" char* get_acc_ext_derivationpath()
{
	return g_acc_ext_derivationpath;	
}

extern "C" char* get_derivationpath()
{
	return g_derivationpath;	
}

extern "C" void set_derivationpath(char* dp, char* acc_ext_dp)
{
	int derivationpath_len = strlen(dp)+1;
	char* old_alloc = g_derivationpath;
	g_derivationpath = (char*)vm_calloc(derivationpath_len);
	strcpy(g_derivationpath, dp);
	if (old_alloc)
	{
		memset(old_alloc, 0, strlen(old_alloc));
		vm_free(old_alloc);
	}	

	derivationpath_len = strlen(acc_ext_dp)+1;
	old_alloc = g_acc_ext_derivationpath;
	g_acc_ext_derivationpath = (char*)vm_calloc(derivationpath_len);
	strcpy(g_acc_ext_derivationpath, acc_ext_dp);
	if (old_alloc)
	{
		memset(old_alloc, 0, strlen(old_alloc));
		vm_free(old_alloc);
	}	
}

extern "C" bool get_hardened()
{
	return g_hardened;	
}

extern "C" void set_hardened(bool hardened)
{
	g_hardened = hardened;
}
extern "C" int get_type()
{
	return g_type;	
}

extern "C" void set_type(int type)
{
	g_type = type;
}

extern "C" void create_priv_key()
{
	if (g_wif)
	{
		char tmp[256];
		char pubkeystr[256];
		char privkeystr[256];
		bool isValid = false;
		char* type_str = "unknown";

		char type_segwit_str[] = "p2wpkh";
		char type_legacy_str[] = "p2pkh";
		char type_nested_segwit_str[] = "p2wpkh-p2sh";

#ifndef TRY_TESTNET
		g_private_key = new PrivateKey(g_wif);
#else
		g_private_key = new PrivateKey(g_wif, &Testnet);
#endif
		isValid = g_private_key->isValid();
		if (isValid == false)
		{
			g_private_key->~PrivateKey();
			g_private_key = NULL;
			return;
		}
		
		if (g_pub_type == TYPE_SEGWIT)
		{
			g_private_key->segwitAddress(tmp, sizeof(tmp));
			type_str = type_segwit_str;
		} else if (g_pub_type == TYPE_LEGACY)
		{
			g_private_key->legacyAddress(tmp, sizeof(tmp));
			type_str = type_legacy_str;
		} else if (g_pub_type == TYPE_NESTED_SEGWIT)
		{
			g_private_key->nestedSegwitAddress(tmp, sizeof(tmp));
			type_str = type_nested_segwit_str;
		}

		sprintf(pubkeystr, "Public key: %s", tmp);
		set_pub(pubkeystr);

		sprintf(privkeystr, "Private key: \r\n%s:%s", type_str, g_wif);
		set_priv(privkeystr);

		memset(pubkeystr, 0, sizeof(pubkeystr));
		memset(privkeystr, 0, sizeof(privkeystr));
		memset(tmp, 0, sizeof(tmp));

		g_private_key->~PrivateKey();
		g_private_key = NULL;
	}
}
extern "C" void derive_xpriv()
{
	char root_priv_str[128];
	char root_priv_seed[130];
	char acc_ext_priv_str[128];
	char bip32_ext_priv_str[128];

	char root_pub_str[128];
	char acc_ext_pub_str[128];
	char bip32_ext_pub_str[128];
	char address[128]; // max len is 90

	bool has_derivation_path = false;
	bool has_acc_ext = false;
	bool is_custom_dp = false;

	memset(root_priv_str, 0, sizeof(root_priv_str));
	memset(root_priv_seed, 0, sizeof(root_priv_seed));
	memset(acc_ext_priv_str, 0, sizeof(acc_ext_priv_str));
	memset(bip32_ext_priv_str, 0, sizeof(bip32_ext_priv_str));
	memset(root_pub_str, 0, sizeof(root_pub_str));
	memset(acc_ext_pub_str, 0, sizeof(acc_ext_pub_str));
	memset(bip32_ext_pub_str, 0, sizeof(bip32_ext_pub_str));
	memset(address, 0, sizeof(address));

	char* passphrase = g_passphrase;
	if (passphrase == NULL)
	{
		passphrase = "";
	}

#ifndef TRY_TESTNET
	g_root_xprivate_key = new HDPrivateKey(g_mnemo, strlen(g_mnemo), passphrase, strlen(passphrase), &Mainnet, NULL);
#else
	g_root_xprivate_key = new HDPrivateKey(g_mnemo, strlen(g_mnemo), passphrase, strlen(passphrase), &Testnet, NULL);
#endif
	
	if (strcmp(g_derivationpath, "m/84'/0'/0'/0") == 0 || strcmp(g_derivationpath, "m/84'/1'/0'/0")== 0)
	{
		g_root_xprivate_key->type = P2WPKH;
	} else if (strcmp(g_derivationpath, "m/44'/0'/0'/0") == 0 || strcmp(g_derivationpath, "m/44'/1'/0'/0")== 0)
	{
		g_root_xprivate_key->type = P2PKH;
	} else if (strcmp(g_derivationpath, "m/49'/0'/0'/0") == 0 || strcmp(g_derivationpath, "m/49'/1'/0'/0")== 0)
	{
		g_root_xprivate_key->type = P2SH_P2WPKH;
	} else {
		is_custom_dp = true;
	}

	g_root_xprivate_key->xprv(root_priv_str, sizeof(root_priv_str));
	g_root_xprivate_key->xpub(root_pub_str, sizeof(root_pub_str));

	toHex(g_root_xprivate_key->seed, 64, root_priv_seed, sizeof(root_priv_seed));

#if 0
	HDPublicKey root_xpublic_key = g_root_xprivate_key->xpub();
#ifdef TRY_TESTNET
	root_xpublic_key.network = &Testnet;
#endif
	root_xpublic_key.xpub(addr, len);
#endif

	if ((g_acc_ext_derivationpath && strcmp(g_acc_ext_derivationpath, "NONE") == 0) || (g_acc_ext_derivationpath == NULL))
	{
		has_acc_ext = false;
	} else {
		//Account Extended Private Key: This represents the extended private key derived with the BIP 44 derivation path m / purpose' / coin_type' / account' / chain / address_index
		//BIP32 Extended Private Key: This represents the extended private key derived from the derivation path m/0'/0'/k' with k being the extended private key.
		HDPrivateKey acc_ext_priv = g_root_xprivate_key->derive(g_acc_ext_derivationpath);
	#ifdef TRY_TESTNET
		acc_ext_priv.network = &Testnet;
	#endif
		acc_ext_priv.xprv(acc_ext_priv_str, sizeof(acc_ext_priv_str));
		acc_ext_priv.xpub(acc_ext_pub_str, sizeof(acc_ext_pub_str));

		acc_ext_priv.~HDPrivateKey();

		has_acc_ext = true;
	}


#if 0
	HDPublicKey acc_ext_pub = acc_ext_priv.xpub();
#ifdef TRY_TESTNET
	acc_ext_pub.network = &Testnet;
#endif
	acc_ext_pub.xpub(addr, len);
#endif

	HDPrivateKey* xprivate_key;
	HDPrivateKey tmp;

	if ((g_derivationpath && strcmp(g_derivationpath, "NONE") == 0) || (g_derivationpath == NULL))
	{
		has_derivation_path = false;
		xprivate_key = g_root_xprivate_key;
	} else {
		has_derivation_path = true;
		tmp = g_root_xprivate_key->derive(g_derivationpath);
		xprivate_key = &tmp;
	}

#ifdef TRY_TESTNET
	xprivate_key->network = &Testnet;
#endif
	xprivate_key->xprv(bip32_ext_priv_str, sizeof(bip32_ext_priv_str)); // xprivate_key
	xprivate_key->xpub(bip32_ext_pub_str, sizeof(bip32_ext_pub_str)); // xpublic_key

	// create info
	// compute len

	char priv_str_1[] = "BIP39 Mnemonic: ";
	char priv_str_2[] = "BIP39 Passphrase: ";
#ifdef TRY_TESTNET
	char priv_str_3[] = "Coin: BTC - Bitcoin (Testnet)";
#else
	char priv_str_3[] = "Coin: BTC - Bitcoin";
#endif
	char priv_str_4[] = "BIP32 Root Private Key: ";
	char priv_str_5[] = "BIP32 Root Public Key: ";
	char priv_str_6[] = "Account Extended Private Key: ";
	char priv_str_7[] = "Account Extended Public Key: ";
	char priv_str_8[] = "BIP32 Derivation Path: ";
	char priv_str_9[] = "BIP32 Extended Private Key: ";
	char priv_str_10[] = "BIP32 Extended Public Key: ";
	char priv_str_11[] = "First 100 Derived Addresses: ";
	char priv_str_12[] = "Hardened: True";
	char priv_str_13[] = "Hardened: False";
	char priv_str_14[] = "BIP39 Seed: ";

	char NEW_LINE[] = "\r\n";
	
	size_t pub_info_len =
		strlen(priv_str_3) + 2 + //include /r/n
		strlen(priv_str_5) +
		strlen(root_pub_str) + 2 + //include /r/n
		strlen(priv_str_7) +
		strlen(acc_ext_pub_str) + 2 + //include /r/n
		strlen(priv_str_8) +
		strlen(g_derivationpath) + 2 + //include /r/n
		strlen(priv_str_13) + 2 + //include /r/n
		strlen(priv_str_10) +
		strlen(bip32_ext_pub_str) + 2 + //include /r/n
		strlen(priv_str_11) + 2 + //include /r/n
		(100 * 93); // 100 addresses, each 90 + /r/n/ + null byte

	size_t priv_info_len = pub_info_len +
		strlen(priv_str_1) +
		strlen(g_mnemo) + 2 + //include /r/n
		strlen(priv_str_2) +
		strlen(passphrase) + 2 + //include /r/n
		strlen(priv_str_14) +
		strlen(root_priv_seed) + 2 + //include /r/n
		strlen(priv_str_4) +
		strlen(root_priv_str) + 2 + //include /r/n
		strlen(priv_str_6) +
		strlen(acc_ext_priv_str) + 2 + //include /r/n
		strlen(priv_str_9) +
		strlen(bip32_ext_priv_str) + 2;

	char* priv_info_str = (char*)vm_calloc(priv_info_len);
	char* pub_info_str = (char*)vm_calloc(pub_info_len);

	// PRIV DATA

	// mnemo
	strcpy(priv_info_str, priv_str_1);
	strcat(priv_info_str, g_mnemo);
	strcat(priv_info_str, NEW_LINE);

	// passphrase
	if (passphrase != NULL && passphrase[0] != 0)
	{
		strcat(priv_info_str, priv_str_2);
		strcat(priv_info_str, passphrase);
		strcat(priv_info_str, NEW_LINE);
	}

	strcat(priv_info_str, priv_str_3);
	strcat(priv_info_str, NEW_LINE);

	// seed
	strcat(priv_info_str, priv_str_14);
	strcat(priv_info_str, root_priv_seed);
	strcat(priv_info_str, NEW_LINE);
	
	strcat(priv_info_str, priv_str_4);
	strcat(priv_info_str, root_priv_str);
	strcat(priv_info_str, NEW_LINE);
	
	strcat(priv_info_str, priv_str_5);
	strcat(priv_info_str, root_pub_str);
	strcat(priv_info_str, NEW_LINE);

	if (has_acc_ext)
	{
		strcat(priv_info_str, priv_str_6);
		strcat(priv_info_str, acc_ext_priv_str);
		strcat(priv_info_str, NEW_LINE);
		
		strcat(priv_info_str, priv_str_7);
		strcat(priv_info_str, acc_ext_pub_str);
		strcat(priv_info_str, NEW_LINE);

	}
	if (has_derivation_path)
	{
		strcat(priv_info_str, priv_str_8);
		strcat(priv_info_str, g_derivationpath);
		strcat(priv_info_str, NEW_LINE);
	}

	if (g_hardened)
	{
		strcat(priv_info_str, priv_str_12);
	} else {
		strcat(priv_info_str, priv_str_13);
	}
	strcat(priv_info_str, NEW_LINE);

	strcat(priv_info_str, priv_str_9);
	strcat(priv_info_str, bip32_ext_priv_str);
	strcat(priv_info_str, NEW_LINE);
	
	strcat(priv_info_str, priv_str_10);
	strcat(priv_info_str, bip32_ext_pub_str);
	strcat(priv_info_str, NEW_LINE);
	
	strcat(priv_info_str, priv_str_11);
	strcat(priv_info_str, NEW_LINE);


	// PUB DATA

	strcpy(pub_info_str, priv_str_3);
	strcat(pub_info_str, NEW_LINE);

	strcat(pub_info_str, priv_str_5);
	strcat(pub_info_str, root_pub_str);
	strcat(pub_info_str, NEW_LINE);

	if (has_acc_ext)
	{
		strcat(pub_info_str, priv_str_7);
		strcat(pub_info_str, acc_ext_pub_str);
		strcat(pub_info_str, NEW_LINE);

	}
	if (has_derivation_path)
	{
		strcat(pub_info_str, priv_str_8);
		strcat(pub_info_str, g_derivationpath);
		strcat(pub_info_str, NEW_LINE);
	}

	if (g_hardened)
	{
		strcat(pub_info_str, priv_str_12);
	} else {
		strcat(pub_info_str, priv_str_13);
	}
	strcat(pub_info_str, NEW_LINE);
	
	strcat(pub_info_str, priv_str_10);
	strcat(pub_info_str, bip32_ext_pub_str);
	strcat(pub_info_str, NEW_LINE);
	
	strcat(pub_info_str, priv_str_11);
	strcat(pub_info_str, NEW_LINE);

	// addresses
	for (int idx = 0; idx < 100; idx++)
	{
		// loop 100
		HDPrivateKey priv_child0 = xprivate_key->child(idx, g_hardened);
	#ifdef TRY_TESTNET
		priv_child0.network = &Testnet;
	#endif
		if (is_custom_dp)
		{
			// force legacy
			priv_child0.legacyAddress(address, sizeof(address));
		} else {
			// auto select
			priv_child0.address(address, sizeof(address));
		}
			
		strcat(priv_info_str, address);
		strcat(priv_info_str, NEW_LINE);
		
		strcat(pub_info_str, address);
		strcat(pub_info_str, NEW_LINE);

		priv_child0.~HDPrivateKey();
	}

	set_xpriv(priv_info_str);
	set_xpub(pub_info_str);

	memset(root_priv_str, 0, sizeof(root_priv_str));
	memset(root_priv_seed, 0, sizeof(root_priv_seed));
	memset(acc_ext_priv_str, 0, sizeof(acc_ext_priv_str));
	memset(bip32_ext_priv_str, 0, sizeof(bip32_ext_priv_str));
	memset(root_pub_str, 0, sizeof(root_pub_str));
	memset(acc_ext_pub_str, 0, sizeof(acc_ext_pub_str));
	memset(bip32_ext_pub_str, 0, sizeof(bip32_ext_pub_str));
	memset(address, 0, sizeof(address));

	if (xprivate_key != g_root_xprivate_key)
	{
		xprivate_key->~HDPrivateKey();
	}
	g_root_xprivate_key->~HDPrivateKey();
	g_root_xprivate_key = NULL;

	memset(priv_info_str, 0, priv_info_len);
	memset(pub_info_str, 0, pub_info_len);
	vm_free(priv_info_str);
	vm_free(pub_info_str);
	
#if 0
	HDPrivateKey priv_child0 = xprivate_key->child(0, g_hardened);
	HDPrivateKey priv_child1 = xprivate_key->child(1, g_hardened);
	
	HDPublicKey child0 = priv_child0.xpub();
	HDPublicKey child1 = priv_child1.xpub();
	
	child0.address(addr, len); // child0
	child1.address(addr, len); // child1
	
	//child0.legacyAddress(addr, len); // child0
	//child1.legacyAddress(addr, len); // child1

	//child0.~HDPublicKey();
	//child1.~HDPublicKey();

	vm_free(addr);
#endif
}


extern "C" char* get_xpub()
{
	return g_xpub;
}

extern "C" char* get_xpriv()
{
	return g_xpriv;
}

extern "C" char* get_pub()
{
	return g_pub;	
}

extern "C" char* get_priv()
{
	return g_priv;	
}

extern "C" void set_xpub(char* str)
{
	int str_len = strlen(str)+1;
	char* old_alloc = g_xpub;
	g_xpub = (char*)vm_calloc(str_len);
	strcpy(g_xpub, str);
	if (old_alloc)
	{
		memset(old_alloc, 0, strlen(old_alloc));
		vm_free(old_alloc);
	}	
}

extern "C" void set_xpriv(char* str)
{
	int str_len = strlen(str)+1;
	char* old_alloc = g_xpriv;
	g_xpriv = (char*)vm_calloc(str_len);
	strcpy(g_xpriv, str);
	if (old_alloc)
	{
		memset(old_alloc, 0, strlen(old_alloc));
		vm_free(old_alloc);
	}	
}

extern "C" void set_priv(char* str)
{
	int str_len = strlen(str)+1;
	char* old_alloc = g_priv;
	g_priv = (char*)vm_calloc(str_len);
	strcpy(g_priv, str);
	if (old_alloc)
	{
		memset(old_alloc, 0, strlen(old_alloc));
		vm_free(old_alloc);
	}	
}

extern "C" void set_pub(char* str)
{
	int str_len = strlen(str)+1;
	char* old_alloc = g_pub;
	g_pub = (char*)vm_calloc(str_len);
	strcpy(g_pub, str);
	if (old_alloc)
	{
		memset(old_alloc, 0, strlen(old_alloc));
		vm_free(old_alloc);
	}	
}

extern "C" size_t c_fromBase64Length(const char * array, size_t arraySize, uint8_t flags)
{
	return fromBase64Length(array, arraySize, flags);
}

extern "C" size_t c_fromBase64(const char * encoded, size_t encodedSize, uint8_t * output, size_t outputSize, uint8_t flags)
{
	return fromBase64(encoded, encodedSize, output, outputSize, flags);
}

extern "C" bool sign_psbt_from_pk(uint8_t* psbt_data, size_t psbt_len, void** signed_psbt_data, size_t* signed_psbt_len)
{
	(*signed_psbt_len) = 0;
	(*signed_psbt_data) = NULL;

	bool res = false;

	bool isValid = false;
#ifndef TRY_TESTNET
	g_private_key = new PrivateKey(g_wif);
#else
	g_private_key = new PrivateKey(g_wif, &Testnet);
#endif
	isValid = g_private_key->isValid();
	if (isValid == false)
	{
		g_private_key->~PrivateKey();
		g_private_key = NULL;
		return false;
	}
		
	PSBT* psbt = NULL;
	PSBT psbt_hex;
	PSBT psbt_raw;
	size_t psbt_parsed = 0;
	
	//+		Streamable	{status=PARSING_FAILED bytes_parsed=1 }	Streamable

	psbt_parsed = psbt_hex.parse(psbt_data, psbt_len, HEX_ENCODING);
	if (psbt_hex.getStatus() == PARSING_DONE)
	{
		psbt = &psbt_hex;
	} else {
		psbt_parsed = psbt_raw.parse(psbt_data, psbt_len, RAW);
		if (psbt_raw.getStatus() == PARSING_DONE)
		{
			psbt = &psbt_raw;
		}
	}
	if (psbt)
	{
		uint8_t counter = psbt->sign(*g_private_key);
		if (counter > 0)
		{
			size_t psbt_serialized = 0;
			size_t psbt_len = 0;
			void* psbt_data = vm_calloc(64);
			size_t toB64 = 0;
		
			do
			{
				psbt_len += 64;
				psbt_data = vm_realloc(psbt_data, psbt_len + 1); // +1 cause serialization adds null byte regardless of max size passed...
				psbt_serialized = psbt->serialize((uint8_t*)psbt_data, psbt_len, RAW);
			} while(psbt_serialized == psbt_len || psbt_serialized == psbt_len+1);

			psbt_len = psbt_serialized;

			(*signed_psbt_len) = toBase64Length((const uint8_t *)psbt_data, psbt_len, BASE64_STANDARD);
			(*signed_psbt_data) = vm_calloc((*signed_psbt_len));
			toB64 = toBase64((const uint8_t *)psbt_data, psbt_len, (char*)(*signed_psbt_data), (*signed_psbt_len), BASE64_STANDARD);
			
			memset(psbt_data, 0, psbt_len);
			vm_free(psbt_data);
			
			res = true;
		}
		psbt->~PSBT();
		psbt = NULL;
	}

	g_private_key->~PrivateKey();
	g_private_key = NULL;
	return res;
}


extern "C" bool sign_psbt_from_hd(uint8_t* psbt_data, size_t psbt_len, void** signed_psbt_data, size_t* signed_psbt_len)
{
	bool res = false;
	bool isValid = false;
	PSBT* psbt = NULL;
	PSBT psbt_hex;
	PSBT psbt_raw;
	size_t psbt_parsed = 0;

	(*signed_psbt_len) = 0;
	(*signed_psbt_data) = NULL;

	char* passphrase = g_passphrase;
	if (passphrase == NULL)
	{
		passphrase = "";
	}

#ifndef TRY_TESTNET
	g_root_xprivate_key = new HDPrivateKey(g_mnemo, strlen(g_mnemo), passphrase, strlen(passphrase), &Mainnet, NULL);
#else
	g_root_xprivate_key = new HDPrivateKey(g_mnemo, strlen(g_mnemo), passphrase, strlen(passphrase), &Testnet, NULL);
#endif
	
	if (strcmp(g_derivationpath, "m/84'/0'/0'/0") == 0 || strcmp(g_derivationpath, "m/84'/1'/0'/0")== 0)
	{
		g_root_xprivate_key->type = P2WPKH;
	} else if (strcmp(g_derivationpath, "m/44'/0'/0'/0") == 0 || strcmp(g_derivationpath, "m/44'/1'/0'/0")== 0)
	{
		g_root_xprivate_key->type = P2PKH;
	} else if (strcmp(g_derivationpath, "m/49'/0'/0'/0") == 0 || strcmp(g_derivationpath, "m/49'/1'/0'/0")== 0)
	{
		g_root_xprivate_key->type = P2SH_P2WPKH;
	} 

	HDPrivateKey* xprivate_key;
	HDPrivateKey tmp;

	if ((g_derivationpath && strcmp(g_derivationpath, "NONE") == 0) || (g_derivationpath == NULL))
	{
		xprivate_key = g_root_xprivate_key;
	} else {
		tmp = g_root_xprivate_key->derive(g_derivationpath);
		xprivate_key = &tmp;
	}
	isValid = xprivate_key->isValid();
	if (isValid == false)
	{
		res = false;
		goto cleanup;
	}

	psbt_parsed = psbt_hex.parse(psbt_data, psbt_len, HEX_ENCODING);
	if (psbt_hex.getStatus() == PARSING_DONE)
	{
		psbt = &psbt_hex;
	} else {
		psbt_parsed = psbt_raw.parse(psbt_data, psbt_len, RAW);
		if (psbt_raw.getStatus() == PARSING_DONE)
		{
			psbt = &psbt_raw;
		}
	}
	if (psbt)
	{
		uint8_t counter = psbt->sign(*xprivate_key);
		if ((g_acc_ext_derivationpath && strcmp(g_acc_ext_derivationpath, "NONE") == 0) || (g_acc_ext_derivationpath == NULL))
		{
			
		} else {
			HDPrivateKey acc_ext_priv = g_root_xprivate_key->derive(g_acc_ext_derivationpath);
		#ifdef TRY_TESTNET
			acc_ext_priv.network = &Testnet;
		#endif
			counter += psbt->sign(acc_ext_priv);
			acc_ext_priv.~HDPrivateKey();
		}

		if (counter > 0)
		{
			size_t psbt_serialized = 0;
			size_t psbt_len = 0;
			void* psbt_data = vm_calloc(64);
			size_t toB64 = 0;
		
			do
			{
				psbt_len += 64;
				psbt_data = vm_realloc(psbt_data, psbt_len + 1); // +1 cause serialization adds null byte regardless of max size passed...
				psbt_serialized = psbt->serialize((uint8_t*)psbt_data, psbt_len, RAW);
			} while(psbt_serialized == psbt_len || psbt_serialized == psbt_len+1);

			psbt_len = psbt_serialized;

			(*signed_psbt_len) = toBase64Length((const uint8_t *)psbt_data, psbt_len, BASE64_STANDARD);
			(*signed_psbt_data) = vm_calloc((*signed_psbt_len));
			toB64 = toBase64((const uint8_t *)psbt_data, psbt_len, (char*)(*signed_psbt_data), (*signed_psbt_len), BASE64_STANDARD);
			
			memset(psbt_data, 0, psbt_len);
			vm_free(psbt_data);

			res = true;
		}
		psbt->~PSBT();
		psbt = NULL;
	}

cleanup:
	if (xprivate_key != g_root_xprivate_key)
	{
		xprivate_key->~HDPrivateKey();
	}
	g_root_xprivate_key->~HDPrivateKey();
	g_root_xprivate_key = NULL;
	return res;
}

extern "C" void cleanup_sensitive_data()
{
	g_hardened = false;
	g_type = TYPE_MNEMONIC;
	if (g_wif)
	{
		memset(g_wif, 0, strlen(g_wif));
		vm_free(g_wif);
		g_wif = NULL;
	}
	if (g_derivationpath)
	{
		memset(g_derivationpath, 0, strlen(g_derivationpath));
		vm_free(g_derivationpath);
		g_derivationpath = NULL;
	}
	if (g_mnemo)
	{
		memset(g_mnemo, 0, strlen(g_mnemo));
		vm_free(g_mnemo);
		g_mnemo = NULL;
	}
	if (g_passphrase)
	{
		memset(g_passphrase, 0, strlen(g_passphrase));
		vm_free(g_passphrase);
		g_passphrase = NULL;
	}

	if (g_pub)
	{
		memset(g_pub, 0, strlen(g_pub));
		vm_free(g_pub);
		g_pub = NULL;
	}

	if (g_priv)
	{
		memset(g_priv, 0, strlen(g_priv));
		vm_free(g_priv);
		g_priv = NULL;
	}

	if (g_xpriv)
	{
		memset(g_xpriv, 0, strlen(g_xpriv));
		vm_free(g_xpriv);
		g_xpriv = NULL;
	}

	if (g_xpub)
	{
		memset(g_xpub, 0, strlen(g_xpub));
		vm_free(g_xpub);
		g_xpub = NULL;
	}

	if (g_root_xprivate_key != NULL)
	{
		g_root_xprivate_key->~HDPrivateKey();
		g_root_xprivate_key = NULL;
	}

	if (g_private_key != NULL) 
	{
		g_private_key->~PrivateKey();
		g_private_key = NULL;
	}
}
