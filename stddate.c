//
//  date.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//

#include "ecc.h"

// MARK: - Private

typedef struct eccdtdate_t eccdtdate_t;
typedef struct eccdttime_t eccdttime_t;

struct eccdtdate_t
{
    int32_t year;
    int32_t month;
    int32_t day;
};

struct eccdttime_t
{
    int32_t h;
    int32_t m;
    int32_t s;
    int32_t ms;
};

static int eccdate_issign(int c);
static void eccdate_setupLocalOffset(void);
static double eccdate_toLocal(double ms);
static double eccdate_toUTC(double ms);
static double eccdate_binaryArgumentOr(eccstate_t *context, int index, double alternative);
static double eccdate_msClip(double ms);
static double eccdate_msFromDate(eccdtdate_t date);
static double eccdate_msFromDateAndTime(eccdtdate_t date, eccdttime_t time);
static double eccdate_msFromArguments(eccstate_t *context);
static double eccdate_msFromBytes(const char *bytes, uint16_t length);
static double eccdate_msToDate(double ms, eccdtdate_t *date);
static void eccdate_msToTime(double ms, eccdttime_t *time);
static void eccdate_msToDateAndTime(double ms, eccdtdate_t *date, eccdttime_t *time);
static double eccdate_msToHours(double ms);
static double eccdate_msToMinutes(double ms);
static double eccdate_msToSeconds(double ms);
static double eccdate_msToMilliseconds(double ms);
static ecccharbuffer_t *eccdate_msToChars(double ms, double offset);
static unsigned int eccdate_msToWeekday(double ms);
static eccvalue_t objdatefn_toString(eccstate_t *context);
static eccvalue_t objdatefn_toUTCString(eccstate_t *context);
static eccvalue_t objdatefn_toJSON(eccstate_t *context);
static eccvalue_t objdatefn_toISOString(eccstate_t *context);
static eccvalue_t objdatefn_toDateString(eccstate_t *context);
static eccvalue_t objdatefn_toTimeString(eccstate_t *context);
static eccvalue_t objdatefn_valueOf(eccstate_t *context);
static eccvalue_t objdatefn_getYear(eccstate_t *context);
static eccvalue_t objdatefn_getFullYear(eccstate_t *context);
static eccvalue_t objdatefn_getUTCFullYear(eccstate_t *context);
static eccvalue_t objdatefn_getMonth(eccstate_t *context);
static eccvalue_t objdatefn_getUTCMonth(eccstate_t *context);
static eccvalue_t objdatefn_getDate(eccstate_t *context);
static eccvalue_t objdatefn_getUTCDate(eccstate_t *context);
static eccvalue_t objdatefn_getDay(eccstate_t *context);
static eccvalue_t objdatefn_getUTCDay(eccstate_t *context);
static eccvalue_t objdatefn_getHours(eccstate_t *context);
static eccvalue_t objdatefn_getUTCHours(eccstate_t *context);
static eccvalue_t objdatefn_getMinutes(eccstate_t *context);
static eccvalue_t objdatefn_getUTCMinutes(eccstate_t *context);
static eccvalue_t objdatefn_getSeconds(eccstate_t *context);
static eccvalue_t objdatefn_getUTCSeconds(eccstate_t *context);
static eccvalue_t objdatefn_getMilliseconds(eccstate_t *context);
static eccvalue_t objdatefn_getUTCMilliseconds(eccstate_t *context);
static eccvalue_t objdatefn_getTimezoneOffset(eccstate_t *context);
static eccvalue_t objdatefn_setTime(eccstate_t *context);
static eccvalue_t objdatefn_setMilliseconds(eccstate_t *context);
static eccvalue_t objdatefn_setUTCMilliseconds(eccstate_t *context);
static eccvalue_t objdatefn_setSeconds(eccstate_t *context);
static eccvalue_t objdatefn_setUTCSeconds(eccstate_t *context);
static eccvalue_t objdatefn_setMinutes(eccstate_t *context);
static eccvalue_t objdatefn_setUTCMinutes(eccstate_t *context);
static eccvalue_t objdatefn_setHours(eccstate_t *context);
static eccvalue_t objdatefn_setUTCHours(eccstate_t *context);
static eccvalue_t objdatefn_setDate(eccstate_t *context);
static eccvalue_t objdatefn_setUTCDate(eccstate_t *context);
static eccvalue_t objdatefn_setMonth(eccstate_t *context);
static eccvalue_t objdatefn_setUTCMonth(eccstate_t *context);
static eccvalue_t objdatefn_setFullYear(eccstate_t *context);
static eccvalue_t objdatefn_setYear(eccstate_t *context);
static eccvalue_t objdatefn_setUTCFullYear(eccstate_t *context);
static eccvalue_t objdatefn_now(eccstate_t *context);
static eccvalue_t objdatefn_parse(eccstate_t *context);
static eccvalue_t objdatefn_UTC(eccstate_t *context);
static eccvalue_t objdatefn_constructor(eccstate_t *context);
static void nsdatefn_setup(void);
static void nsdatefn_teardown(void);
static eccobjdate_t *nsdatefn_create(double ms);


eccobject_t* ECC_Prototype_Date = NULL;
eccobjfunction_t* ECC_CtorFunc_Date = NULL;

const eccobjinterntype_t ECC_Type_Date = {
    .text = &ECC_ConstString_DateType,
};

static double localOffset;

static const double msPerSecond = 1000;
static const double msPerMinute = 60000;
static const double msPerHour = 3600000;
static const double msPerDay = 86400000;

const struct eccpseudonsdate_t ECCNSDate = {
    nsdatefn_setup,
    nsdatefn_teardown,
    nsdatefn_create,
    {},
};

static int eccdate_issign(int c)
{
    return c == '+' || c == '-';
}

static void eccdate_setupLocalOffset(void)
{
    struct tm tm = {
        .tm_mday = 2,
        .tm_year = 70,
        .tm_wday = 5,
        .tm_isdst = -1,
    };
    time_t time = mktime(&tm);
    if(time < 0)
        localOffset = -0.;
    else
        localOffset = difftime(86400, time) / 3600.;
}

static double eccdate_toLocal(double ms)
{
    return ms + localOffset * msPerHour;
}

static double eccdate_toUTC(double ms)
{
    return ms - localOffset * msPerHour;
}

static double eccdate_binaryArgumentOr(eccstate_t* context, int index, double alternative)
{
    eccvalue_t value = ECCNSContext.argument(context, index);
    if(value.check == 1)
        return ECCNSValue.toBinary(context, ECCNSContext.argument(context, index)).data.binary;
    else
        return alternative;
}

static double eccdate_msClip(double ms)
{
    if(!isfinite(ms) || fabs(ms) > 8.64 * pow(10, 15))
        return NAN;
    else
        return ms;
}

static double eccdate_msFromDate(eccdtdate_t date)
{
    // Low-Level ECCNSDate Algorithms, http://howardhinnant.github.io/date_algorithms.html#days_from_civil

    int32_t era;
    uint32_t yoe, doy, doe;

    date.year -= date.month <= 2;
    era = (int32_t)((date.year >= 0 ? date.year : date.year - 399) / 400);
    yoe = (uint32_t)(date.year - era * 400);// [0, 399]
    doy = (153 * (date.month + (date.month > 2 ? -3 : 9)) + 2) / 5 + date.day - 1;// [0, 365]
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;// [0, 146096]

    return (double)(era * 146097 + (int32_t)(doe)-719468) * msPerDay;
}

static double eccdate_msFromDateAndTime(eccdtdate_t date, eccdttime_t time)
{
    return eccdate_msFromDate(date) + time.h * msPerHour + time.m * msPerMinute + time.s * msPerSecond + time.ms;
}

static double eccdate_msFromArguments(eccstate_t* context)
{
    uint16_t count;
    eccdtdate_t date;
    eccdttime_t time;
    double year, month, day, h, m, s, ms;

    count = ECCNSContext.argumentCount(context);

    year = ECCNSValue.toBinary(context, ECCNSContext.argument(context, 0)).data.binary,
    month = ECCNSValue.toBinary(context, ECCNSContext.argument(context, 1)).data.binary,
    day = count > 2 ? ECCNSValue.toBinary(context, ECCNSContext.argument(context, 2)).data.binary : 1,
    h = count > 3 ? ECCNSValue.toBinary(context, ECCNSContext.argument(context, 3)).data.binary : 0,
    m = count > 4 ? ECCNSValue.toBinary(context, ECCNSContext.argument(context, 4)).data.binary : 0,
    s = count > 5 ? ECCNSValue.toBinary(context, ECCNSContext.argument(context, 5)).data.binary : 0,
    ms = count > 6 ? ECCNSValue.toBinary(context, ECCNSContext.argument(context, 6)).data.binary : 0;

    if(isnan(year) || isnan(month) || isnan(day) || isnan(h) || isnan(m) || isnan(s) || isnan(ms))
        return NAN;
    else
    {
        if(year >= 0 && year <= 99)
            year += 1900;

        date.year = year;
        date.month = month + 1;
        date.day = day;
        time.h = h;
        time.m = m;
        time.s = s;
        time.ms = ms;
        return eccdate_msFromDateAndTime(date, time);
    }
}

static double eccdate_msFromBytes(const char* bytes, uint16_t length)
{
    char buffer[length + 1];
    int n = 0, nOffset = 0, i = 0;
    int year, month = 1, day = 1, h = 0, m = 0, s = 0, ms = 0, hOffset = 0, mOffset = 0;
    eccdtdate_t date;
    eccdttime_t time;

    if(!length)
        return NAN;

    memcpy(buffer, bytes, length);
    buffer[length] = '\0';

    n = 0, i = sscanf(buffer,
                      eccdate_issign(buffer[0]) ? "%07d"
                                          "%n"
                                          "-%02d"
                                          "%n"
                                          "-%02d%n"
                                          "T%02d:%02d%n"
                                          ":%02d%n"
                                          ".%03d%n" :
                                          "%04d"
                                          "%n"
                                          "-%02d"
                                          "%n"
                                          "-%02d%n"
                                          "T%02d:%02d%n"
                                          ":%02d%n"
                                          ".%03d%n",
                      &year, &n, &month, &n, &day, &n, &h, &m, &n, &s, &n, &ms, &n);

    if(i <= 3)
    {
        if(n == length)
            goto done;

        /*vvv*/
    }
    else if(i >= 5)
    {
        if(buffer[n] == 'Z' && n + 1 == length)
            goto done;
        else if(eccdate_issign(buffer[n]) && sscanf(buffer + n, "%03d:%02d%n", &hOffset, &mOffset, &nOffset) == 2 && n + nOffset == length)
            goto done;
        else
            return NAN;
    }
    else
        return NAN;

    hOffset = localOffset;
    n = 0, i = sscanf(buffer,
                      "%d"
                      "/%02d"
                      "/%02d%n"
                      " %02d:%02d%n"
                      ":%02d%n",
                      &year, &month, &day, &n, &h, &m, &n, &s, &n);

    if(year < 100)
        return NAN;

    if(n == length)
        goto done;
    else if(i >= 5 && buffer[n++] == ' ' && eccdate_issign(buffer[n]) && sscanf(buffer + n, "%03d%02d%n", &hOffset, &mOffset, &nOffset) == 2 && n + nOffset == length)
        goto done;
    else
        return NAN;

done:
    if(month <= 0 || day <= 0 || h < 0 || m < 0 || s < 0 || ms < 0 || hOffset < -12 || mOffset < 0)
        return NAN;

    if(month > 12 || day > 31 || h > 23 || m > 59 || s > 59 || ms > 999 || hOffset > 14 || mOffset > 59)
        return NAN;

    date.year = year;
    date.month = month;
    date.day = day;
    time.h = h;
    time.m = m;
    time.s = s;
    time.ms = ms;
    return eccdate_msFromDateAndTime(date, time) - (hOffset * 60 + mOffset) * msPerMinute;
}

static double eccdate_msToDate(double ms, eccdtdate_t* date)
{
    // Low-Level ECCNSDate Algorithms, http://howardhinnant.github.io/date_algorithms.html#civil_from_days

    int32_t z, era;
    uint32_t doe, yoe, doy, mp;

    z = ms / msPerDay + 719468;
    era = (z >= 0 ? z : z - 146096) / 146097;
    doe = (z - era * 146097);// [0, 146096]
    yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;// [0, 399]
    doy = doe - (365 * yoe + yoe / 4 - yoe / 100);// [0, 365]
    mp = (5 * doy + 2) / 153;// [0, 11]
    date->day = doy - (153 * mp + 2) / 5 + 1;// [1, 31]
    date->month = mp + (mp < 10 ? 3 : -9);// [1, 12]
    date->year = (int32_t)(yoe) + era * 400 + (mp >= 10);

    return fmod(ms, 24 * msPerHour) + (ms < 0 ? 24 * msPerHour : 0);
}

static void eccdate_msToTime(double ms, eccdttime_t* time)
{
    time->h = fmod(floor(ms / msPerHour), 24);
    time->m = fmod(floor(ms / msPerMinute), 60);
    time->s = fmod(floor(ms / msPerSecond), 60);
    time->ms = fmod(ms, 1000);
}

static void eccdate_msToDateAndTime(double ms, eccdtdate_t* date, eccdttime_t* time)
{
    eccdate_msToTime(eccdate_msToDate(ms, date), time);
}

static double eccdate_msToHours(double ms)
{
    eccdtdate_t date;
    eccdttime_t time;
    eccdate_msToDateAndTime(ms, &date, &time);
    return time.h;
}

static double eccdate_msToMinutes(double ms)
{
    eccdtdate_t date;
    eccdttime_t time;
    eccdate_msToDateAndTime(ms, &date, &time);
    return time.m;
}

static double eccdate_msToSeconds(double ms)
{
    eccdtdate_t date;
    eccdttime_t time;
    eccdate_msToDateAndTime(ms, &date, &time);
    return time.s;
}

static double eccdate_msToMilliseconds(double ms)
{
    eccdtdate_t date;
    eccdttime_t time;
    eccdate_msToDateAndTime(ms, &date, &time);
    return time.ms;
}

static ecccharbuffer_t* eccdate_msToChars(double ms, double offset)
{
    const char* format;
    eccdtdate_t date;
    eccdttime_t time;

    if(isnan(ms))
        return ECCNSChars.create("Invalid Date");

    eccdate_msToDateAndTime(ms + offset * msPerHour, &date, &time);

    if(date.year >= 100 && date.year <= 9999)
        format = "%04d/%02d/%02d %02d:%02d:%02d %+03d%02d";
    else
        format = "%+06d-%02d-%02dT%02d:%02d:%02d%+03d:%02d";

    return ECCNSChars.create(format, date.year, date.month, date.day, time.h, time.m, time.s, (int)offset, (int)fmod(fabs(offset) * 60, 60));
}

static unsigned int eccdate_msToWeekday(double ms)
{
    int z = ms / msPerDay;
    return (unsigned)(z >= -4 ? (z + 4) % 7 : (z + 5) % 7 + 6);
}

// MARK: - Static Members

static eccvalue_t objdatefn_toString(eccstate_t* context)
{
    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    return ECCNSValue.chars(eccdate_msToChars(context->thisvalue.data.date->ms, localOffset));
}

static eccvalue_t objdatefn_toUTCString(eccstate_t* context)
{
    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    return ECCNSValue.chars(eccdate_msToChars(context->thisvalue.data.date->ms, 0));
}

static eccvalue_t objdatefn_toJSON(eccstate_t* context)
{
    eccvalue_t object = ECCNSValue.toObject(context, ECCNSContext.getThis(context));
    eccvalue_t tv = ECCNSValue.toPrimitive(context, object, ECC_VALHINT_NUMBER);
    eccvalue_t toISO;

    if(tv.type == ECC_VALTYPE_BINARY && !isfinite(tv.data.binary))
        return ECCValConstNull;

    toISO = ECCNSObject.getMember(context, object.data.object, ECC_ConstKey_toISOString);
    if(toISO.type != ECC_VALTYPE_FUNCTION)
    {
        ECCNSContext.setTextIndex(context, ECC_CTXINDEXTYPE_CALL);
        ECCNSContext.typeError(context, ECCNSChars.create("toISOString is not a function"));
    }
    return ECCNSContext.callFunction(context, toISO.data.function, object, 0);
}

static eccvalue_t objdatefn_toISOString(eccstate_t* context)
{
    const char* format;
    eccdtdate_t date;
    eccdttime_t time;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    if(isnan(context->thisvalue.data.date->ms))
        ECCNSContext.rangeError(context, ECCNSChars.create("invalid date"));

    eccdate_msToDateAndTime(context->thisvalue.data.date->ms, &date, &time);

    if(date.year >= 0 && date.year <= 9999)
        format = "%04d-%02d-%02dT%02d:%02d:%06.3fZ";
    else
        format = "%+06d-%02d-%02dT%02d:%02d:%06.3fZ";

    return ECCNSValue.chars(ECCNSChars.create(format, date.year, date.month, date.day, time.h, time.m, time.s + (time.ms / 1000.)));
}

static eccvalue_t objdatefn_toDateString(eccstate_t* context)
{
    eccdtdate_t date;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    if(isnan(context->thisvalue.data.date->ms))
        return ECCNSValue.chars(ECCNSChars.create("Invalid Date"));

    eccdate_msToDate(eccdate_toLocal(context->thisvalue.data.date->ms), &date);

    return ECCNSValue.chars(ECCNSChars.create("%04d/%02d/%02d", date.year, date.month, date.day));
}

static eccvalue_t objdatefn_toTimeString(eccstate_t* context)
{
    eccdttime_t time;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    if(isnan(context->thisvalue.data.date->ms))
        return ECCNSValue.chars(ECCNSChars.create("Invalid Date"));

    eccdate_msToTime(eccdate_toLocal(context->thisvalue.data.date->ms), &time);

    return ECCNSValue.chars(ECCNSChars.create("%02d:%02d:%02d %+03d%02d", time.h, time.m, time.s, (int)localOffset, (int)fmod(fabs(localOffset) * 60, 60)));
}

static eccvalue_t objdatefn_valueOf(eccstate_t* context)
{
    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    return ECCNSValue.binary(context->thisvalue.data.date->ms >= 0 ? floor(context->thisvalue.data.date->ms) : ceil(context->thisvalue.data.date->ms));
}

static eccvalue_t objdatefn_getYear(eccstate_t* context)
{
    eccdtdate_t date;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    eccdate_msToDate(eccdate_toLocal(context->thisvalue.data.date->ms), &date);
    return ECCNSValue.binary(date.year - 1900);
}

static eccvalue_t objdatefn_getFullYear(eccstate_t* context)
{
    eccdtdate_t date;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    eccdate_msToDate(eccdate_toLocal(context->thisvalue.data.date->ms), &date);
    return ECCNSValue.binary(date.year);
}

static eccvalue_t objdatefn_getUTCFullYear(eccstate_t* context)
{
    eccdtdate_t date;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    eccdate_msToDate(context->thisvalue.data.date->ms, &date);
    return ECCNSValue.binary(date.year);
}

static eccvalue_t objdatefn_getMonth(eccstate_t* context)
{
    eccdtdate_t date;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    eccdate_msToDate(eccdate_toLocal(context->thisvalue.data.date->ms), &date);
    return ECCNSValue.binary(date.month - 1);
}

static eccvalue_t objdatefn_getUTCMonth(eccstate_t* context)
{
    eccdtdate_t date;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    eccdate_msToDate(context->thisvalue.data.date->ms, &date);
    return ECCNSValue.binary(date.month - 1);
}

static eccvalue_t objdatefn_getDate(eccstate_t* context)
{
    eccdtdate_t date;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    eccdate_msToDate(eccdate_toLocal(context->thisvalue.data.date->ms), &date);
    return ECCNSValue.binary(date.day);
}

static eccvalue_t objdatefn_getUTCDate(eccstate_t* context)
{
    eccdtdate_t date;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    eccdate_msToDate(context->thisvalue.data.date->ms, &date);
    return ECCNSValue.binary(date.day);
}

static eccvalue_t objdatefn_getDay(eccstate_t* context)
{
    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    return ECCNSValue.binary(eccdate_msToWeekday(eccdate_toLocal(context->thisvalue.data.date->ms)));
}

static eccvalue_t objdatefn_getUTCDay(eccstate_t* context)
{
    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    return ECCNSValue.binary(eccdate_msToWeekday(context->thisvalue.data.date->ms));
}

static eccvalue_t objdatefn_getHours(eccstate_t* context)
{
    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    return ECCNSValue.binary(eccdate_msToHours(eccdate_toLocal(context->thisvalue.data.date->ms)));
}

static eccvalue_t objdatefn_getUTCHours(eccstate_t* context)
{
    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    return ECCNSValue.binary(eccdate_msToHours(context->thisvalue.data.date->ms));
}

static eccvalue_t objdatefn_getMinutes(eccstate_t* context)
{
    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    return ECCNSValue.binary(eccdate_msToMinutes(eccdate_toLocal(context->thisvalue.data.date->ms)));
}

static eccvalue_t objdatefn_getUTCMinutes(eccstate_t* context)
{
    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    return ECCNSValue.binary(eccdate_msToMinutes(context->thisvalue.data.date->ms));
}

static eccvalue_t objdatefn_getSeconds(eccstate_t* context)
{
    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    return ECCNSValue.binary(eccdate_msToSeconds(eccdate_toLocal(context->thisvalue.data.date->ms)));
}

static eccvalue_t objdatefn_getUTCSeconds(eccstate_t* context)
{
    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    return ECCNSValue.binary(eccdate_msToSeconds(context->thisvalue.data.date->ms));
}

static eccvalue_t objdatefn_getMilliseconds(eccstate_t* context)
{
    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    return ECCNSValue.binary(eccdate_msToMilliseconds(eccdate_toLocal(context->thisvalue.data.date->ms)));
}

static eccvalue_t objdatefn_getUTCMilliseconds(eccstate_t* context)
{
    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    return ECCNSValue.binary(eccdate_msToMilliseconds(context->thisvalue.data.date->ms));
}

static eccvalue_t objdatefn_getTimezoneOffset(eccstate_t* context)
{
    (void)context;
    return ECCNSValue.binary(-localOffset * 60);
}

static eccvalue_t objdatefn_setTime(eccstate_t* context)
{
    double ms;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    ms = ECCNSValue.toBinary(context, ECCNSContext.argument(context, 0)).data.binary;

    return ECCNSValue.binary(context->thisvalue.data.date->ms = eccdate_msClip(ms));
}

static eccvalue_t objdatefn_setMilliseconds(eccstate_t* context)
{
    eccdtdate_t date;
    eccdttime_t time;
    double ms;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    eccdate_msToDateAndTime(eccdate_toLocal(context->thisvalue.data.date->ms), &date, &time);
    ms = eccdate_binaryArgumentOr(context, 0, NAN);
    if(isnan(ms))
        return ECCNSValue.binary(context->thisvalue.data.date->ms = NAN);

    time.ms = ms;
    return ECCNSValue.binary(context->thisvalue.data.date->ms = eccdate_msClip(eccdate_toUTC(eccdate_msFromDateAndTime(date, time))));
}

static eccvalue_t objdatefn_setUTCMilliseconds(eccstate_t* context)
{
    eccdtdate_t date;
    eccdttime_t time;
    double ms;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    eccdate_msToDateAndTime(context->thisvalue.data.date->ms, &date, &time);
    ms = eccdate_binaryArgumentOr(context, 0, NAN);
    if(isnan(ms))
        return ECCNSValue.binary(context->thisvalue.data.date->ms = NAN);

    time.ms = ms;
    return ECCNSValue.binary(context->thisvalue.data.date->ms = eccdate_msClip(eccdate_msFromDateAndTime(date, time)));
}

static eccvalue_t objdatefn_setSeconds(eccstate_t* context)
{
    eccdtdate_t date;
    eccdttime_t time;
    double s, ms;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    eccdate_msToDateAndTime(eccdate_toLocal(context->thisvalue.data.date->ms), &date, &time);
    s = eccdate_binaryArgumentOr(context, 0, NAN);
    ms = eccdate_binaryArgumentOr(context, 1, time.ms);
    if(isnan(s) || isnan(ms))
        return ECCNSValue.binary(context->thisvalue.data.date->ms = NAN);

    time.s = s;
    time.ms = ms;
    return ECCNSValue.binary(context->thisvalue.data.date->ms = eccdate_msClip(eccdate_toUTC(eccdate_msFromDateAndTime(date, time))));
}

static eccvalue_t objdatefn_setUTCSeconds(eccstate_t* context)
{
    eccdtdate_t date;
    eccdttime_t time;
    double s, ms;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    eccdate_msToDateAndTime(context->thisvalue.data.date->ms, &date, &time);
    s = eccdate_binaryArgumentOr(context, 0, NAN);
    ms = eccdate_binaryArgumentOr(context, 1, time.ms);
    if(isnan(s) || isnan(ms))
        return ECCNSValue.binary(context->thisvalue.data.date->ms = NAN);

    time.s = s;
    time.ms = ms;
    return ECCNSValue.binary(context->thisvalue.data.date->ms = eccdate_msClip(eccdate_msFromDateAndTime(date, time)));
}

static eccvalue_t objdatefn_setMinutes(eccstate_t* context)
{
    eccdtdate_t date;
    eccdttime_t time;
    double m, s, ms;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    eccdate_msToDateAndTime(eccdate_toLocal(context->thisvalue.data.date->ms), &date, &time);
    m = eccdate_binaryArgumentOr(context, 0, NAN);
    s = eccdate_binaryArgumentOr(context, 1, time.s);
    ms = eccdate_binaryArgumentOr(context, 2, time.ms);
    if(isnan(m) || isnan(s) || isnan(ms))
        return ECCNSValue.binary(context->thisvalue.data.date->ms = NAN);

    time.m = m;
    time.s = s;
    time.ms = ms;
    return ECCNSValue.binary(context->thisvalue.data.date->ms = eccdate_msClip(eccdate_toUTC(eccdate_msFromDateAndTime(date, time))));
}

static eccvalue_t objdatefn_setUTCMinutes(eccstate_t* context)
{
    eccdtdate_t date;
    eccdttime_t time;
    double m, s, ms;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    eccdate_msToDateAndTime(context->thisvalue.data.date->ms, &date, &time);
    m = eccdate_binaryArgumentOr(context, 0, NAN);
    s = eccdate_binaryArgumentOr(context, 1, time.s);
    ms = eccdate_binaryArgumentOr(context, 2, time.ms);
    if(isnan(m) || isnan(s) || isnan(ms))
        return ECCNSValue.binary(context->thisvalue.data.date->ms = NAN);

    time.m = m;
    time.s = s;
    time.ms = ms;
    return ECCNSValue.binary(context->thisvalue.data.date->ms = eccdate_msClip(eccdate_msFromDateAndTime(date, time)));
}

static eccvalue_t objdatefn_setHours(eccstate_t* context)
{
    eccdtdate_t date;
    eccdttime_t time;
    double h, m, s, ms;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    eccdate_msToDateAndTime(eccdate_toLocal(context->thisvalue.data.date->ms), &date, &time);
    h = eccdate_binaryArgumentOr(context, 0, NAN);
    m = eccdate_binaryArgumentOr(context, 1, time.m);
    s = eccdate_binaryArgumentOr(context, 2, time.s);
    ms = eccdate_binaryArgumentOr(context, 3, time.ms);
    if(isnan(h) || isnan(m) || isnan(s) || isnan(ms))
        return ECCNSValue.binary(context->thisvalue.data.date->ms = NAN);

    time.h = h;
    time.m = m;
    time.s = s;
    time.ms = ms;
    return ECCNSValue.binary(context->thisvalue.data.date->ms = eccdate_msClip(eccdate_toUTC(eccdate_msFromDateAndTime(date, time))));
}

static eccvalue_t objdatefn_setUTCHours(eccstate_t* context)
{
    eccdtdate_t date;
    eccdttime_t time;
    double h, m, s, ms;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    eccdate_msToDateAndTime(context->thisvalue.data.date->ms, &date, &time);
    h = eccdate_binaryArgumentOr(context, 0, NAN);
    m = eccdate_binaryArgumentOr(context, 1, time.m);
    s = eccdate_binaryArgumentOr(context, 2, time.s);
    ms = eccdate_binaryArgumentOr(context, 3, time.ms);
    if(isnan(h) || isnan(m) || isnan(s) || isnan(ms))
        return ECCNSValue.binary(context->thisvalue.data.date->ms = NAN);

    time.h = h;
    time.m = m;
    time.s = s;
    time.ms = ms;
    return ECCNSValue.binary(context->thisvalue.data.date->ms = eccdate_msClip(eccdate_msFromDateAndTime(date, time)));
}

static eccvalue_t objdatefn_setDate(eccstate_t* context)
{
    eccdtdate_t date;
    double day, ms;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    ms = eccdate_msToDate(eccdate_toLocal(context->thisvalue.data.date->ms), &date);
    day = eccdate_binaryArgumentOr(context, 0, NAN);
    if(isnan(day))
        return ECCNSValue.binary(context->thisvalue.data.date->ms = NAN);

    date.day = day;
    return ECCNSValue.binary(context->thisvalue.data.date->ms = eccdate_msClip(eccdate_toUTC(ms + eccdate_msFromDate(date))));
}

static eccvalue_t objdatefn_setUTCDate(eccstate_t* context)
{
    eccdtdate_t date;
    double day, ms;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    ms = eccdate_msToDate(context->thisvalue.data.date->ms, &date);
    day = eccdate_binaryArgumentOr(context, 0, NAN);
    if(isnan(day))
        return ECCNSValue.binary(context->thisvalue.data.date->ms = NAN);

    date.day = day;
    return ECCNSValue.binary(context->thisvalue.data.date->ms = eccdate_msClip(ms + eccdate_msFromDate(date)));
}

static eccvalue_t objdatefn_setMonth(eccstate_t* context)
{
    eccdtdate_t date;
    double month, day, ms;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    ms = eccdate_msToDate(eccdate_toLocal(context->thisvalue.data.date->ms), &date);
    month = eccdate_binaryArgumentOr(context, 0, NAN) + 1;
    day = eccdate_binaryArgumentOr(context, 1, date.day);
    if(isnan(month) || isnan(day))
        return ECCNSValue.binary(context->thisvalue.data.date->ms = NAN);

    date.month = month;
    date.day = day;
    return ECCNSValue.binary(context->thisvalue.data.date->ms = eccdate_msClip(eccdate_toUTC(ms + eccdate_msFromDate(date))));
}

static eccvalue_t objdatefn_setUTCMonth(eccstate_t* context)
{
    eccdtdate_t date;
    double month, day, ms;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    ms = eccdate_msToDate(context->thisvalue.data.date->ms, &date);
    month = eccdate_binaryArgumentOr(context, 0, NAN) + 1;
    day = eccdate_binaryArgumentOr(context, 1, date.day);
    if(isnan(month) || isnan(day))
        return ECCNSValue.binary(context->thisvalue.data.date->ms = NAN);

    date.month = month;
    date.day = day;
    return ECCNSValue.binary(context->thisvalue.data.date->ms = eccdate_msClip(ms + eccdate_msFromDate(date)));
}

static eccvalue_t objdatefn_setFullYear(eccstate_t* context)
{
    eccdtdate_t date;
    double year, month, day, ms;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    if(isnan(context->thisvalue.data.date->ms))
        context->thisvalue.data.date->ms = 0;

    ms = eccdate_msToDate(eccdate_toLocal(context->thisvalue.data.date->ms), &date);
    year = eccdate_binaryArgumentOr(context, 0, NAN);
    month = eccdate_binaryArgumentOr(context, 1, date.month - 1) + 1;
    day = eccdate_binaryArgumentOr(context, 2, date.day);
    if(isnan(year) || isnan(month) || isnan(day))
        return ECCNSValue.binary(context->thisvalue.data.date->ms = NAN);

    date.year = year;
    date.month = month;
    date.day = day;
    return ECCNSValue.binary(context->thisvalue.data.date->ms = eccdate_msClip(eccdate_toUTC(ms + eccdate_msFromDate(date))));
}

static eccvalue_t objdatefn_setYear(eccstate_t* context)
{
    eccdtdate_t date;
    double year, month, day, ms;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    if(isnan(context->thisvalue.data.date->ms))
        context->thisvalue.data.date->ms = 0;

    ms = eccdate_msToDate(eccdate_toLocal(context->thisvalue.data.date->ms), &date);
    year = eccdate_binaryArgumentOr(context, 0, NAN);
    month = eccdate_binaryArgumentOr(context, 1, date.month - 1) + 1;
    day = eccdate_binaryArgumentOr(context, 2, date.day);
    if(isnan(year) || isnan(month) || isnan(day))
        return ECCNSValue.binary(context->thisvalue.data.date->ms = NAN);

    date.year = year < 100 ? year + 1900 : year;
    date.month = month;
    date.day = day;
    return ECCNSValue.binary(context->thisvalue.data.date->ms = eccdate_msClip(eccdate_toUTC(ms + eccdate_msFromDate(date))));
}

static eccvalue_t objdatefn_setUTCFullYear(eccstate_t* context)
{
    eccdtdate_t date;
    double year, month, day, ms;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_DATE);

    if(isnan(context->thisvalue.data.date->ms))
        context->thisvalue.data.date->ms = 0;

    ms = eccdate_msToDate(context->thisvalue.data.date->ms, &date);
    year = eccdate_binaryArgumentOr(context, 0, NAN);
    month = eccdate_binaryArgumentOr(context, 1, date.month - 1) + 1;
    day = eccdate_binaryArgumentOr(context, 2, date.day);
    if(isnan(year) || isnan(month) || isnan(day))
        return ECCNSValue.binary(context->thisvalue.data.date->ms = NAN);

    date.year = year;
    date.month = month;
    date.day = day;
    return ECCNSValue.binary(context->thisvalue.data.date->ms = eccdate_msClip(ms + eccdate_msFromDate(date)));
}

static eccvalue_t objdatefn_now(eccstate_t* context)
{
    (void)context;
    return ECCNSValue.binary(eccdate_msClip(ECCNSEnv.currentTime()));
}

static eccvalue_t objdatefn_parse(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSValue.toString(context, ECCNSContext.argument(context, 0));

    return ECCNSValue.binary(eccdate_msClip(eccdate_msFromBytes(ECCNSValue.stringBytes(&value), ECCNSValue.stringLength(&value))));
}

static eccvalue_t objdatefn_UTC(eccstate_t* context)
{
    if(ECCNSContext.argumentCount(context) > 1)
        return ECCNSValue.binary(eccdate_msClip(eccdate_msFromArguments(context)));

    return ECCNSValue.binary(NAN);
}

static eccvalue_t objdatefn_constructor(eccstate_t* context)
{
    double time;
    uint16_t count;

    if(!context->construct)
        return ECCNSValue.chars(eccdate_msToChars(ECCNSEnv.currentTime(), localOffset));

    count = ECCNSContext.argumentCount(context);

    if(count > 1)
        time = eccdate_toUTC(eccdate_msFromArguments(context));
    else if(count)
    {
        eccvalue_t value = ECCNSValue.toPrimitive(context, ECCNSContext.argument(context, 0), ECC_VALHINT_AUTO);

        if(ECCNSValue.isString(value))
            time = eccdate_msFromBytes(ECCNSValue.stringBytes(&value), ECCNSValue.stringLength(&value));
        else
            time = ECCNSValue.toBinary(context, value).data.binary;
    }
    else
        time = ECCNSEnv.currentTime();

    return ECCNSValue.date(nsdatefn_create(time));
}

static void nsdatefn_setup(void)
{
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;

    eccdate_setupLocalOffset();

    ECCNSFunction.setupBuiltinObject(&ECC_CtorFunc_Date, objdatefn_constructor, -7, &ECC_Prototype_Date, ECCNSValue.date(nsdatefn_create(NAN)), &ECC_Type_Date);

    ECCNSFunction.addMethod(ECC_CtorFunc_Date, "parse", objdatefn_parse, 1, h);
    ECCNSFunction.addMethod(ECC_CtorFunc_Date, "UTC", objdatefn_UTC, -7, h);
    ECCNSFunction.addMethod(ECC_CtorFunc_Date, "now", objdatefn_now, 0, h);

    ECCNSFunction.addToObject(ECC_Prototype_Date, "toString", objdatefn_toString, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "toDateString", objdatefn_toDateString, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "toTimeString", objdatefn_toTimeString, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "toLocaleString", objdatefn_toString, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "toLocaleDateString", objdatefn_toDateString, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "toLocaleTimeString", objdatefn_toTimeString, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "valueOf", objdatefn_valueOf, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "getTime", objdatefn_valueOf, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "getYear", objdatefn_getYear, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "getFullYear", objdatefn_getFullYear, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "getUTCFullYear", objdatefn_getUTCFullYear, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "getMonth", objdatefn_getMonth, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "getUTCMonth", objdatefn_getUTCMonth, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "getDate", objdatefn_getDate, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "getUTCDate", objdatefn_getUTCDate, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "getDay", objdatefn_getDay, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "getUTCDay", objdatefn_getUTCDay, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "getHours", objdatefn_getHours, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "getUTCHours", objdatefn_getUTCHours, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "getMinutes", objdatefn_getMinutes, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "getUTCMinutes", objdatefn_getUTCMinutes, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "getSeconds", objdatefn_getSeconds, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "getUTCSeconds", objdatefn_getUTCSeconds, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "getMilliseconds", objdatefn_getMilliseconds, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "getUTCMilliseconds", objdatefn_getUTCMilliseconds, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "getTimezoneOffset", objdatefn_getTimezoneOffset, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "setTime", objdatefn_setTime, 1, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "setMilliseconds", objdatefn_setMilliseconds, 1, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "setUTCMilliseconds", objdatefn_setUTCMilliseconds, 1, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "setSeconds", objdatefn_setSeconds, 2, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "setUTCSeconds", objdatefn_setUTCSeconds, 2, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "setMinutes", objdatefn_setMinutes, 3, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "setUTCMinutes", objdatefn_setUTCMinutes, 3, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "setHours", objdatefn_setHours, 4, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "setUTCHours", objdatefn_setUTCHours, 4, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "setDate", objdatefn_setDate, 1, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "setUTCDate", objdatefn_setUTCDate, 1, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "setMonth", objdatefn_setMonth, 2, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "setUTCMonth", objdatefn_setUTCMonth, 2, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "setYear", objdatefn_setYear, 3, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "setFullYear", objdatefn_setFullYear, 3, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "setUTCFullYear", objdatefn_setUTCFullYear, 3, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "toUTCString", objdatefn_toUTCString, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "toGMTString", objdatefn_toUTCString, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "toISOString", objdatefn_toISOString, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Date, "toJSON", objdatefn_toJSON, 1, h);
}

static void nsdatefn_teardown(void)
{
    ECC_Prototype_Date = NULL;
    ECC_CtorFunc_Date = NULL;
}

static eccobjdate_t* nsdatefn_create(double ms)
{
    eccobjdate_t* self = malloc(sizeof(*self));
    *self = ECCNSDate.identity;
    ECCNSMemoryPool.addObject(&self->object);
    ECCNSObject.initialize(&self->object, ECC_Prototype_Date);

    self->ms = eccdate_msClip(ms);

    return self;
}
