#include "brands.h"
#include "brand_registry.h"

// ─── Forward-declare every brand defined in its own .cpp ─────────────────────

extern const BrandEntry BRAND_AMAZON_FIRE_TV;
extern const BrandEntry BRAND_AOC;
extern const BrandEntry BRAND_BANG_OLUFSEN;
extern const BrandEntry BRAND_BEKO;
extern const BrandEntry BRAND_BENQ;
extern const BrandEntry BRAND_CHANGHONG;
extern const BrandEntry BRAND_COBY;
extern const BrandEntry BRAND_DYNEX;
extern const BrandEntry BRAND_ELEMENT;
extern const BrandEntry BRAND_EMERSON;
extern const BrandEntry BRAND_FUNAI;
extern const BrandEntry BRAND_GRUNDIG;
extern const BrandEntry BRAND_HAIER;
extern const BrandEntry BRAND_HISENSE;
extern const BrandEntry BRAND_HITACHI;
extern const BrandEntry BRAND_IFFALCON;
extern const BrandEntry BRAND_INSIGNIA;
extern const BrandEntry BRAND_JVC;
extern const BrandEntry BRAND_KONKA;
extern const BrandEntry BRAND_LG;
extern const BrandEntry BRAND_LOEWE;
extern const BrandEntry BRAND_MAGNAVOX;
extern const BrandEntry BRAND_MEDION;
extern const BrandEntry BRAND_METZ;
extern const BrandEntry BRAND_MITSUBISHI;
extern const BrandEntry BRAND_NOKIA_TV;
extern const BrandEntry BRAND_ONN;
extern const BrandEntry BRAND_PANASONIC;
extern const BrandEntry BRAND_PHILIPS;
extern const BrandEntry BRAND_PIONEER;
extern const BrandEntry BRAND_POLAROID;
extern const BrandEntry BRAND_PROSCAN;
extern const BrandEntry BRAND_RCA;
extern const BrandEntry BRAND_ROKU_TV;
extern const BrandEntry BRAND_SAMSUNG;
extern const BrandEntry BRAND_SANYO;
extern const BrandEntry BRAND_SCEPTRE;
extern const BrandEntry BRAND_SEIKI;
extern const BrandEntry BRAND_SHARP;
extern const BrandEntry BRAND_SKYWORTH;
extern const BrandEntry BRAND_SONY;
extern const BrandEntry BRAND_TCL;
extern const BrandEntry BRAND_THOMSON;
extern const BrandEntry BRAND_TOSHIBA;
extern const BrandEntry BRAND_VESTEL;
extern const BrandEntry BRAND_VIZIO;
extern const BrandEntry BRAND_WESTINGHOUSE;
extern const BrandEntry BRAND_XIAOMI;

// ─── Master table (order must match TV_BRAND_NAMES[] in tv_remote.h) ─────────

const BrandEntry* const TV_BRANDS[] = {
    &BRAND_AMAZON_FIRE_TV,
    &BRAND_AOC,
    &BRAND_BANG_OLUFSEN,
    &BRAND_BEKO,
    &BRAND_BENQ,
    &BRAND_CHANGHONG,
    &BRAND_COBY,
    &BRAND_DYNEX,
    &BRAND_ELEMENT,
    &BRAND_EMERSON,
    &BRAND_FUNAI,
    &BRAND_GRUNDIG,
    &BRAND_HAIER,
    &BRAND_HISENSE,
    &BRAND_HITACHI,
    &BRAND_IFFALCON,
    &BRAND_INSIGNIA,
    &BRAND_JVC,
    &BRAND_KONKA,
    &BRAND_LG,
    &BRAND_LOEWE,
    &BRAND_MAGNAVOX,
    &BRAND_MEDION,
    &BRAND_METZ,
    &BRAND_MITSUBISHI,
    &BRAND_NOKIA_TV,
    &BRAND_ONN,
    &BRAND_PANASONIC,
    &BRAND_PHILIPS,
    &BRAND_PIONEER,
    &BRAND_POLAROID,
    &BRAND_PROSCAN,
    &BRAND_RCA,
    &BRAND_ROKU_TV,
    &BRAND_SAMSUNG,
    &BRAND_SANYO,
    &BRAND_SCEPTRE,
    &BRAND_SEIKI,
    &BRAND_SHARP,
    &BRAND_SKYWORTH,
    &BRAND_SONY,
    &BRAND_TCL,
    &BRAND_THOMSON,
    &BRAND_TOSHIBA,
    &BRAND_VESTEL,
    &BRAND_VIZIO,
    &BRAND_WESTINGHOUSE,
    &BRAND_XIAOMI,
};

const int TV_BRAND_COUNT = (int)(sizeof(TV_BRANDS) / sizeof(TV_BRANDS[0]));
