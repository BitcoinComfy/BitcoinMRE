#ifndef __MRE_MENU__
#define __MRE_MENU__

#include "vmsys.h"
#include "vmio.h"
#include "vmgraph.h"
#include "vmchset.h"
#include "vmstdlib.h"
#include "vmche.h"
#include "ResID.h"
#include "vm4res.h"
#include "vmlog.h"


enum views {
	NK_MRE_VIEW_NONE,				// no action if this view is selected
	NK_MRE_VIEW_WELCOME,			// welcome screen
	NK_MRE_VIEW_ENTROPY_GEN,		// extra input from user for extra entropy
	NK_MRE_VIEW_ENTROPY_GEN_WAIT,	// wait for entropy computation (mic rec, file/photo hash, imei, imsi, operator code, etc...)
	NK_MRE_VIEW_WALLET_ACTION,		// select wallet, recovery wallet, create new one, settings...
	NK_MRE_VIEW_WALLET_UNLOCK,		// unlock wallet with pin
	NK_MRE_VIEW_WALLET_UNLOCK_PIN,	// ask for pin
	NK_MRE_VIEW_WALLET_RECOVERY,	// wif / number of mnemonics
	NK_MRE_VIEW_WALLET_CREATION_1,	// number of mnemonics
	NK_MRE_VIEW_WALLET_CREATION_2,	// warning
	NK_MRE_VIEW_WALLET_CREATION_3,	// display seed
	NK_MRE_VIEW_WALLET_CREATION_4,	// create pin
	NK_MRE_VIEW_WALLET_CREATION_5,	// wallet name -> save
	NK_MRE_VIEW_WALLET_PKEY,		// display private key
	NK_MRE_VIEW_WALLET_PKEY_CONFIRM,// confirm private key
	NK_MRE_VIEW_WALLET_PIN,			// insert pin
	NK_MRE_VIEW_WALLET_PIN_CONFIRM,	// confirm pin
	NK_MRE_VIEW_WALLET_MAIN_1,		// extra passphrase
	NK_MRE_VIEW_WALLET_MAIN_2,		// select derivation path
	NK_MRE_VIEW_WALLET_MAIN_3,		// open wallet actions (display xpub qr, save xpub on sdcard, sign PSBT, fetch UTXOs via Internet or SMS maybe)
	NK_MRE_VIEW_WALLET_WIF_1,		// select legacy, segwit, nested segwit
	NK_MRE_VIEW_WALLET_WIF_2,		// open wallet wif actions
	NK_MRE_VIEW_WARNING_SENSITIVE,	// sensitive data about to be displayed
	NK_MRE_VIEW_DISPLAY_SENSITIVE,	// display sensitive data
	NK_MRE_VIEW_DISPLAY_XPUB_QR,	// display xpub qr
	NK_MRE_VIEW_SAVE_XPUB_SDCARD,	// save xpub on sdcard
	NK_MRE_VIEW_SIGN_PSBT,			// sign PSBT
	NK_MRE_VIEW_WALLET_PSBT_SIGNED, // psbt signed and saved
	NK_MRE_VIEW_WALLET_PSBT_ERROR,	// psbt signing error
	NK_MRE_VIEW_SENSITIVE_SAVE, // warning about saving sensitive info (priv keys)
	NK_MRE_VIEW_SENSITIVE_SAVED,	// sensitive data saved

	NK_MRE_VIEW_QR_TEST,			// read qr decoded text

	// implement later on...maybe   //
	NK_MRE_VIEW_SETTINGS,			// settings (Internet/SMS RPC servers)
	NK_MRE_VIEW_FETCH_UTXO_BALANCE_INTERNET,	// fetch UTXOs via Internet
	NK_MRE_VIEW_FETCH_UTXO_BALANCE_SMS,			// fetch UTXOs via SMS (diffie-hellman over SMS, emergency mode or phone without internet)
	NK_MRE_VIEW_TX_CREATE_IN,		// create tx: select the input UTXOs and the relative amounts (coin control)
	NK_MRE_VIEW_TX_CREATE_OUT,		// create tx: select the output addresess (including change) and the relative amounts, tx fee, signature method, rbf maybe
	NK_MRE_VIEW_TX_CONFIRM_FEE,		// confirm fee creation
	NK_MRE_VIEW_TX_CREATE_CONFIRM,	// confirm tx creation
	NK_MRE_VIEW_TX_SEND,			// send tx (choose, sdcard, internet, sms)
	NK_MRE_VIEW_TX_STATUS,			// query tx status (choose, internet, sms)
	//////////////////////////////////
	NK_MRE_VIEW_TESTTEST,			// for testing and debugging
	NK_MRE_VIEW_MAX_ID				// last entry, just to have the max id
};

void update_gui();
void set_view(int view);

void mre_set_global_data (void);

#endif // __MRE_MENU__