// Generated dynamically from maan_ki_baat_quote_library.md. Do not edit directly.
#ifndef QUOTE_LIBRARY_H
#define QUOTE_LIBRARY_H

#include <Arduino.h>

struct Quote {
  const char* id;
  const char* category;
  const char* text;
};

// Total quotes detected: 50
const Quote quotes_db[] PROGMEM = {
  { "Q001", "Prem / Mohabbat", "Chaand se kya poochhna, raat kiski hai." },
  { "Q002", "Prem / Mohabbat", "Tum mile toh laga, intezaar bhi khoobsurat tha." },
  { "Q003", "Prem / Mohabbat", "Ishq mein hisaab rakha, toh ishq kahan raha." },
  { "Q004", "Prem / Mohabbat", "Kuch log milkar bhi nahi milte, kuch door rehkar bhi saath hote hain." },
  { "Q005", "Prem / Mohabbat", "Mohabbat chehra nahi dekhti, bas ek nazar mein ghar bana leti hai." },
  { "Q006", "Prem / Mohabbat", "Uski khamoshi bhi, meri kahani ka hissa thi." },
  { "Q007", "Prem / Mohabbat", "Jise paana zaroori tha, usse kho kar samjha ki zaroori kya tha." },
  { "Q008", "Prem / Mohabbat", "Raat gehri thi, ya yaadein zyada thi." },
  { "Q009", "Zindagi / Waqt", "Waqt sabko mila, kisi ne jeeya, kisi ne guzaar diya." },
  { "Q010", "Zindagi / Waqt", "Zindagi chhoti nahi thi, hum hi jaldi mein the." },
  { "Q011", "Zindagi / Waqt", "Jo badal raha hai, wahi toh zinda hai." },
  { "Q012", "Zindagi / Waqt", "Kal ki fikr mein aaj ko mat khona." },
  { "Q013", "Zindagi / Waqt", "Har cheez mil jaaye, toh talab kis baat ki?" },
  { "Q014", "Zindagi / Waqt", "Kuch kamiyaan hi toh insaan ko insaan rakhti hain." },
  { "Q015", "Zindagi / Waqt", "Hum waqt bachate rahe, waqt humein badalta raha." },
  { "Q016", "Zindagi / Waqt", "Raasta wahi tha, nazariya badal gaya." },
  { "Q017", "Deep / Philosophical", "Aaina sach bolta hai, bas hum sunna nahi chahte." },
  { "Q018", "Deep / Philosophical", "Khamoshi mein aksar sabse zyada shor hota hai." },
  { "Q019", "Deep / Philosophical", "Kuch sawaal, jawab se zyada khoobsurat hote hain." },
  { "Q020", "Deep / Philosophical", "Hum raaste dhoondte rahe, manzil toh andar baithi thi." },
  { "Q021", "Deep / Philosophical", "Jitna khud ko jaana, utna duniya se kam mila." },
  { "Q022", "Deep / Philosophical", "Sab kuch paas tha, bas sukoon door tha." },
  { "Q023", "Deep / Philosophical", "Jo samajh aa gaya, woh zindagi nahi thi." },
  { "Q024", "Deep / Philosophical", "Insaan ghar banata raha, aur waqt use mehmaan banata raha." },
  { "Q025", "Deep / Philosophical", "Duniya chehra padhti hai, waqt niyat." },
  { "Q026", "Spiritual / Desi", "Bhagwan ko dhoondhte rahe, insaan ko dekhna bhool gaye." },
  { "Q027", "Spiritual / Desi", "Karm chup rehta hai, hisaab nahi." },
  { "Q028", "Spiritual / Desi", "Mann shaant ho, toh mitti bhi mandir lagti hai." },
  { "Q029", "Spiritual / Desi", "Prarthana lafzon se nahi, niyat se hoti hai." },
  { "Q030", "Spiritual / Desi", "Jo dena seekh gaya, woh kabhi gareeb nahi raha." },
  { "Q031", "Spiritual / Desi", "Ahankaar bada tha, insaan chhota pad gaya." },
  { "Q032", "Spiritual / Desi", "Sukoon bazaar mein nahi milta, andar ugaana padta hai." },
  { "Q033", "Spiritual / Desi", "Jitna chhoda, utna halka hota gaya." },
  { "Q034", "Khamoshi / Yaadein", "Kuch log yaad nahi aate, bas bhoolte nahi." },
  { "Q035", "Khamoshi / Yaadein", "Hum theek hain, bas pehle jaise nahi." },
  { "Q036", "Khamoshi / Yaadein", "Jo kehna tha, woh khamoshi keh gayi." },
  { "Q037", "Khamoshi / Yaadein", "Kabhi kabhi kisi ka na hona, uske hone se zyada mehsoos hota hai." },
  { "Q038", "Khamoshi / Yaadein", "Yaadein bhi ajeeb hain, waqt ke saath purani nahi hoti." },
  { "Q039", "Khamoshi / Yaadein", "Kuch rishton ka ant nahi hota, bas baatein band ho jaati hain." },
  { "Q040", "Khamoshi / Yaadein", "Woh badla nahi tha, bas humari jagah badal gayi thi." },
  { "Q041", "Raat / Chaand", "Chaand akela tha, phir bhi poori raat roshan thi." },
  { "Q042", "Raat / Chaand", "Raat chup thi, dil bol raha tha." },
  { "Q043", "Raat / Chaand", "Sitaron ko dekhkar laga, andhera bhi khoobsurat ho sakta hai." },
  { "Q044", "Raat / Chaand", "Chaand kisi ka nahi, phir bhi sabka lagta hai." },
  { "Q045", "Raat / Chaand", "Kuch raatein sone ke liye nahi, samajhne ke liye hoti hain." },
  { "Q046", "Witty / Observational", "Zindagi samajhne nikle the, chai pe ruk gaye." },
  { "Q047", "Witty / Observational", "Dil bachana tha, ishq kar baithe." },
  { "Q048", "Witty / Observational", "Sukoon chahiye tha, notifications band kar diye." },
  { "Q049", "Witty / Observational", "Plan bahut the, zindagi ko kuch aur manzoor tha." },
  { "Q050", "Witty / Observational", "Dimag ne kaha ruk ja, dil ne kaha dekhte hain." },
};

const size_t quotes_count = 50;

#endif // QUOTE_LIBRARY_H
