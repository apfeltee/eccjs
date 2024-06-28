
/*
//  date.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
*/

#include "ecc.h"
#include "compat.h"

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
static double eccdate_binaryArgumentOr(ecccontext_t *context, int index, double alternative);
static double eccdate_msClip(double ms);
static double eccdate_msFromDate(eccdtdate_t date);
static double eccdate_msFromDateAndTime(eccdtdate_t date, eccdttime_t time);
static double eccdate_msFromArguments(ecccontext_t *context);
static double eccdate_msFromBytes(const char *bytes, uint32_t length);
static double eccdate_msToDate(double ms, eccdtdate_t *date);
static void eccdate_msToTime(double ms, eccdttime_t *time);
static void eccdate_msToDateAndTime(double ms, eccdtdate_t *date, eccdttime_t *time);
static double eccdate_msToHours(double ms);
static double eccdate_msToMinutes(double ms);
static double eccdate_msToSeconds(double ms);
static double eccdate_msToMilliseconds(double ms);
static eccstrbuffer_t *eccdate_msToChars(double ms, double offset);
static unsigned int eccdate_msToWeekday(double ms);
static eccvalue_t objdatefn_toString(ecccontext_t *context);
static eccvalue_t objdatefn_toUTCString(ecccontext_t *context);
static eccvalue_t objdatefn_toJSON(ecccontext_t *context);
static eccvalue_t objdatefn_toISOString(ecccontext_t *context);
static eccvalue_t objdatefn_toDateString(ecccontext_t *context);
static eccvalue_t objdatefn_toTimeString(ecccontext_t *context);
static eccvalue_t objdatefn_valueOf(ecccontext_t *context);
static eccvalue_t objdatefn_getYear(ecccontext_t *context);
static eccvalue_t objdatefn_getFullYear(ecccontext_t *context);
static eccvalue_t objdatefn_getUTCFullYear(ecccontext_t *context);
static eccvalue_t objdatefn_getMonth(ecccontext_t *context);
static eccvalue_t objdatefn_getUTCMonth(ecccontext_t *context);
static eccvalue_t objdatefn_getDate(ecccontext_t *context);
static eccvalue_t objdatefn_getUTCDate(ecccontext_t *context);
static eccvalue_t objdatefn_getDay(ecccontext_t *context);
static eccvalue_t objdatefn_getUTCDay(ecccontext_t *context);
static eccvalue_t objdatefn_getHours(ecccontext_t *context);
static eccvalue_t objdatefn_getUTCHours(ecccontext_t *context);
static eccvalue_t objdatefn_getMinutes(ecccontext_t *context);
static eccvalue_t objdatefn_getUTCMinutes(ecccontext_t *context);
static eccvalue_t objdatefn_getSeconds(ecccontext_t *context);
static eccvalue_t objdatefn_getUTCSeconds(ecccontext_t *context);
static eccvalue_t objdatefn_getMilliseconds(ecccontext_t *context);
static eccvalue_t objdatefn_getUTCMilliseconds(ecccontext_t *context);
static eccvalue_t objdatefn_getTimezoneOffset(ecccontext_t *context);
static eccvalue_t objdatefn_setTime(ecccontext_t *context);
static eccvalue_t objdatefn_setMilliseconds(ecccontext_t *context);
static eccvalue_t objdatefn_setUTCMilliseconds(ecccontext_t *context);
static eccvalue_t objdatefn_setSeconds(ecccontext_t *context);
static eccvalue_t objdatefn_setUTCSeconds(ecccontext_t *context);
static eccvalue_t objdatefn_setMinutes(ecccontext_t *context);
static eccvalue_t objdatefn_setUTCMinutes(ecccontext_t *context);
static eccvalue_t objdatefn_setHours(ecccontext_t *context);
static eccvalue_t objdatefn_setUTCHours(ecccontext_t *context);
static eccvalue_t objdatefn_setDate(ecccontext_t *context);
static eccvalue_t objdatefn_setUTCDate(ecccontext_t *context);
static eccvalue_t objdatefn_setMonth(ecccontext_t *context);
static eccvalue_t objdatefn_setUTCMonth(ecccontext_t *context);
static eccvalue_t objdatefn_setFullYear(ecccontext_t *context);
static eccvalue_t objdatefn_setYear(ecccontext_t *context);
static eccvalue_t objdatefn_setUTCFullYear(ecccontext_t *context);
static eccvalue_t objdatefn_now(ecccontext_t *context);
static eccvalue_t objdatefn_parse(ecccontext_t *context);
static eccvalue_t objdatefn_UTC(ecccontext_t *context);
static eccvalue_t objdatefn_constructor(ecccontext_t *context);

eccobject_t* ECC_Prototype_Date = NULL;
eccobjfunction_t* ECC_CtorFunc_Date = NULL;

const eccobjinterntype_t ECC_Type_Date = {
    .text = &ECC_String_DateType,
};

static double localOffset;

static const double msPerSecond = 1000;
static const double msPerMinute = 60000;
static const double msPerHour = 3600000;
static const double msPerDay = 86400000;

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

static double eccdate_binaryArgumentOr(ecccontext_t* context, int index, double alternative)
{
    eccvalue_t value = ecc_context_argument(context, index);
    if(value.check == 1)
        return ecc_value_tobinary(context, ecc_context_argument(context, index)).data.valnumfloat;
    else
        return alternative;
}

static double eccdate_msClip(double ms)
{
    if(!isfinite(ms) || fabs(ms) > 8.64 * pow(10, 15))
        return ECC_CONST_NAN;
    else
        return ms;
}

static double eccdate_msFromDate(eccdtdate_t date)
{
    /*
    * Low-Level Date Algorithms, http://howardhinnant.github.io/date_algorithms.html#days_from_civil
    */

    int32_t era;
    uint32_t yoe, doy, doe;

    date.year -= date.month <= 2;
    era = (int32_t)((date.year >= 0 ? date.year : date.year - 399) / 400);
    /* [0, 399] */
    yoe = (uint32_t)(date.year - era * 400);
    /* [0, 365] */
    doy = (153 * (date.month + (date.month > 2 ? -3 : 9)) + 2) / 5 + date.day - 1;
    /* [0, 146096] */
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;

    return (double)(era * 146097 + (int32_t)(doe)-719468) * msPerDay;
}

static double eccdate_msFromDateAndTime(eccdtdate_t date, eccdttime_t time)
{
    return eccdate_msFromDate(date) + time.h * msPerHour + time.m * msPerMinute + time.s * msPerSecond + time.ms;
}

static double eccdate_msFromArguments(ecccontext_t* context)
{
    uint32_t count;
    eccdtdate_t date;
    eccdttime_t time;
    double year, month, day, h, m, s, ms;

    count = ecc_context_argumentcount(context);

    year = ecc_value_tobinary(context, ecc_context_argument(context, 0)).data.valnumfloat,
    month = ecc_value_tobinary(context, ecc_context_argument(context, 1)).data.valnumfloat,
    day = count > 2 ? ecc_value_tobinary(context, ecc_context_argument(context, 2)).data.valnumfloat : 1,
    h = count > 3 ? ecc_value_tobinary(context, ecc_context_argument(context, 3)).data.valnumfloat : 0,
    m = count > 4 ? ecc_value_tobinary(context, ecc_context_argument(context, 4)).data.valnumfloat : 0,
    s = count > 5 ? ecc_value_tobinary(context, ecc_context_argument(context, 5)).data.valnumfloat : 0,
    ms = count > 6 ? ecc_value_tobinary(context, ecc_context_argument(context, 6)).data.valnumfloat : 0;

    if(isnan(year) || isnan(month) || isnan(day) || isnan(h) || isnan(m) || isnan(s) || isnan(ms))
        return ECC_CONST_NAN;
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

static double eccdate_msFromBytes(const char* bytes, uint32_t length)
{
    char buffer[length + 1];
    int n = 0, nOffset = 0, i = 0;
    int year, month = 1, day = 1, h = 0, m = 0, s = 0, ms = 0, hOffset = 0, mOffset = 0;
    eccdtdate_t date;
    eccdttime_t time;

    if(!length)
        return ECC_CONST_NAN;

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
            return ECC_CONST_NAN;
    }
    else
        return ECC_CONST_NAN;

    hOffset = localOffset;
    n = 0, i = sscanf(buffer,
                      "%d"
                      "/%02d"
                      "/%02d%n"
                      " %02d:%02d%n"
                      ":%02d%n",
                      &year, &month, &day, &n, &h, &m, &n, &s, &n);

    if(year < 100)
        return ECC_CONST_NAN;

    if(n == length)
        goto done;
    else if(i >= 5 && buffer[n++] == ' ' && eccdate_issign(buffer[n]) && sscanf(buffer + n, "%03d%02d%n", &hOffset, &mOffset, &nOffset) == 2 && n + nOffset == length)
        goto done;
    else
        return ECC_CONST_NAN;

done:
    if(month <= 0 || day <= 0 || h < 0 || m < 0 || s < 0 || ms < 0 || hOffset < -12 || mOffset < 0)
        return ECC_CONST_NAN;

    if(month > 12 || day > 31 || h > 23 || m > 59 || s > 59 || ms > 999 || hOffset > 14 || mOffset > 59)
        return ECC_CONST_NAN;

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
    int32_t z, era;
    uint32_t doe, yoe, doy, mp;

    z = ms / msPerDay + 719468;
    era = (z >= 0 ? z : z - 146096) / 146097;
    /* [0, 146096] */
    doe = (z - era * 146097);
    /* [0, 399] */
    yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    /* [0, 365] */
    doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    /* [0, 11] */
    mp = (5 * doy + 2) / 153;
    /* [1, 31] */
    date->day = doy - (153 * mp + 2) / 5 + 1;
    /* [1, 12] */
    date->month = mp + (mp < 10 ? 3 : -9);
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

static eccstrbuffer_t* eccdate_msToChars(double ms, double offset)
{
    const char* format;
    eccdtdate_t date;
    eccdttime_t time;

    if(isnan(ms))
        return ecc_strbuf_create("Invalid Date");

    eccdate_msToDateAndTime(ms + offset * msPerHour, &date, &time);

    if(date.year >= 100 && date.year <= 9999)
        format = "%04d/%02d/%02d %02d:%02d:%02d %+03d%02d";
    else
        format = "%+06d-%02d-%02dT%02d:%02d:%02d%+03d:%02d";

    return ecc_strbuf_create(format, date.year, date.month, date.day, time.h, time.m, time.s, (int)offset, (int)fmod(fabs(offset) * 60, 60));
}

static unsigned int eccdate_msToWeekday(double ms)
{
    int z = ms / msPerDay;
    return (unsigned)(z >= -4 ? (z + 4) % 7 : (z + 5) % 7 + 6);
}

static eccvalue_t objdatefn_toString(ecccontext_t* context)
{
    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    return ecc_value_fromchars(eccdate_msToChars(context->thisvalue.data.date->ms, localOffset));
}

static eccvalue_t objdatefn_toUTCString(ecccontext_t* context)
{
    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    return ecc_value_fromchars(eccdate_msToChars(context->thisvalue.data.date->ms, 0));
}

static eccvalue_t objdatefn_toJSON(ecccontext_t* context)
{
    eccvalue_t object = ecc_value_toobject(context, ecc_context_this(context));
    eccvalue_t tv = ecc_value_toprimitive(context, object, ECC_VALHINT_NUMBER);
    eccvalue_t toISO;

    if(tv.type == ECC_VALTYPE_BINARY && !isfinite(tv.data.valnumfloat))
        return ECCValConstNull;

    toISO = ecc_object_getmember(context, object.data.object, ECC_ConstKey_toISOString);
    if(toISO.type != ECC_VALTYPE_FUNCTION)
    {
        ecc_context_settextindex(context, ECC_CTXINDEXTYPE_CALL);
        ecc_context_typeerror(context, ecc_strbuf_create("toISOString is not a function"));
    }
    return ecc_context_callfunction(context, toISO.data.function, object, 0);
}

static eccvalue_t objdatefn_toISOString(ecccontext_t* context)
{
    const char* format;
    eccdtdate_t date;
    eccdttime_t time;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    if(isnan(context->thisvalue.data.date->ms))
        ecc_context_rangeerror(context, ecc_strbuf_create("invalid date"));

    eccdate_msToDateAndTime(context->thisvalue.data.date->ms, &date, &time);

    if(date.year >= 0 && date.year <= 9999)
        format = "%04d-%02d-%02dT%02d:%02d:%06.3fZ";
    else
        format = "%+06d-%02d-%02dT%02d:%02d:%06.3fZ";

    return ecc_value_fromchars(ecc_strbuf_create(format, date.year, date.month, date.day, time.h, time.m, time.s + (time.ms / 1000.)));
}

static eccvalue_t objdatefn_toDateString(ecccontext_t* context)
{
    eccdtdate_t date;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    if(isnan(context->thisvalue.data.date->ms))
        return ecc_value_fromchars(ecc_strbuf_create("Invalid Date"));

    eccdate_msToDate(eccdate_toLocal(context->thisvalue.data.date->ms), &date);

    return ecc_value_fromchars(ecc_strbuf_create("%04d/%02d/%02d", date.year, date.month, date.day));
}

static eccvalue_t objdatefn_toTimeString(ecccontext_t* context)
{
    eccdttime_t time;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    if(isnan(context->thisvalue.data.date->ms))
        return ecc_value_fromchars(ecc_strbuf_create("Invalid Date"));

    eccdate_msToTime(eccdate_toLocal(context->thisvalue.data.date->ms), &time);

    return ecc_value_fromchars(ecc_strbuf_create("%02d:%02d:%02d %+03d%02d", time.h, time.m, time.s, (int)localOffset, (int)fmod(fabs(localOffset) * 60, 60)));
}

static eccvalue_t objdatefn_valueOf(ecccontext_t* context)
{
    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    return ecc_value_fromfloat(context->thisvalue.data.date->ms >= 0 ? floor(context->thisvalue.data.date->ms) : ceil(context->thisvalue.data.date->ms));
}

static eccvalue_t objdatefn_getYear(ecccontext_t* context)
{
    eccdtdate_t date;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    eccdate_msToDate(eccdate_toLocal(context->thisvalue.data.date->ms), &date);
    return ecc_value_fromfloat(date.year - 1900);
}

static eccvalue_t objdatefn_getFullYear(ecccontext_t* context)
{
    eccdtdate_t date;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    eccdate_msToDate(eccdate_toLocal(context->thisvalue.data.date->ms), &date);
    return ecc_value_fromfloat(date.year);
}

static eccvalue_t objdatefn_getUTCFullYear(ecccontext_t* context)
{
    eccdtdate_t date;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    eccdate_msToDate(context->thisvalue.data.date->ms, &date);
    return ecc_value_fromfloat(date.year);
}

static eccvalue_t objdatefn_getMonth(ecccontext_t* context)
{
    eccdtdate_t date;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    eccdate_msToDate(eccdate_toLocal(context->thisvalue.data.date->ms), &date);
    return ecc_value_fromfloat(date.month - 1);
}

static eccvalue_t objdatefn_getUTCMonth(ecccontext_t* context)
{
    eccdtdate_t date;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    eccdate_msToDate(context->thisvalue.data.date->ms, &date);
    return ecc_value_fromfloat(date.month - 1);
}

static eccvalue_t objdatefn_getDate(ecccontext_t* context)
{
    eccdtdate_t date;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    eccdate_msToDate(eccdate_toLocal(context->thisvalue.data.date->ms), &date);
    return ecc_value_fromfloat(date.day);
}

static eccvalue_t objdatefn_getUTCDate(ecccontext_t* context)
{
    eccdtdate_t date;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    eccdate_msToDate(context->thisvalue.data.date->ms, &date);
    return ecc_value_fromfloat(date.day);
}

static eccvalue_t objdatefn_getDay(ecccontext_t* context)
{
    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    return ecc_value_fromfloat(eccdate_msToWeekday(eccdate_toLocal(context->thisvalue.data.date->ms)));
}

static eccvalue_t objdatefn_getUTCDay(ecccontext_t* context)
{
    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    return ecc_value_fromfloat(eccdate_msToWeekday(context->thisvalue.data.date->ms));
}

static eccvalue_t objdatefn_getHours(ecccontext_t* context)
{
    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    return ecc_value_fromfloat(eccdate_msToHours(eccdate_toLocal(context->thisvalue.data.date->ms)));
}

static eccvalue_t objdatefn_getUTCHours(ecccontext_t* context)
{
    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    return ecc_value_fromfloat(eccdate_msToHours(context->thisvalue.data.date->ms));
}

static eccvalue_t objdatefn_getMinutes(ecccontext_t* context)
{
    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    return ecc_value_fromfloat(eccdate_msToMinutes(eccdate_toLocal(context->thisvalue.data.date->ms)));
}

static eccvalue_t objdatefn_getUTCMinutes(ecccontext_t* context)
{
    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    return ecc_value_fromfloat(eccdate_msToMinutes(context->thisvalue.data.date->ms));
}

static eccvalue_t objdatefn_getSeconds(ecccontext_t* context)
{
    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    return ecc_value_fromfloat(eccdate_msToSeconds(eccdate_toLocal(context->thisvalue.data.date->ms)));
}

static eccvalue_t objdatefn_getUTCSeconds(ecccontext_t* context)
{
    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    return ecc_value_fromfloat(eccdate_msToSeconds(context->thisvalue.data.date->ms));
}

static eccvalue_t objdatefn_getMilliseconds(ecccontext_t* context)
{
    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    return ecc_value_fromfloat(eccdate_msToMilliseconds(eccdate_toLocal(context->thisvalue.data.date->ms)));
}

static eccvalue_t objdatefn_getUTCMilliseconds(ecccontext_t* context)
{
    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    return ecc_value_fromfloat(eccdate_msToMilliseconds(context->thisvalue.data.date->ms));
}

static eccvalue_t objdatefn_getTimezoneOffset(ecccontext_t* context)
{
    (void)context;
    return ecc_value_fromfloat(-localOffset * 60);
}

static eccvalue_t objdatefn_setTime(ecccontext_t* context)
{
    double ms;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    ms = ecc_value_tobinary(context, ecc_context_argument(context, 0)).data.valnumfloat;

    return ecc_value_fromfloat(context->thisvalue.data.date->ms = eccdate_msClip(ms));
}

static eccvalue_t objdatefn_setMilliseconds(ecccontext_t* context)
{
    eccdtdate_t date;
    eccdttime_t time;
    double ms;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    eccdate_msToDateAndTime(eccdate_toLocal(context->thisvalue.data.date->ms), &date, &time);
    ms = eccdate_binaryArgumentOr(context, 0, ECC_CONST_NAN);
    if(isnan(ms))
        return ecc_value_fromfloat(context->thisvalue.data.date->ms = ECC_CONST_NAN);

    time.ms = ms;
    return ecc_value_fromfloat(context->thisvalue.data.date->ms = eccdate_msClip(eccdate_toUTC(eccdate_msFromDateAndTime(date, time))));
}

static eccvalue_t objdatefn_setUTCMilliseconds(ecccontext_t* context)
{
    eccdtdate_t date;
    eccdttime_t time;
    double ms;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    eccdate_msToDateAndTime(context->thisvalue.data.date->ms, &date, &time);
    ms = eccdate_binaryArgumentOr(context, 0, ECC_CONST_NAN);
    if(isnan(ms))
        return ecc_value_fromfloat(context->thisvalue.data.date->ms = ECC_CONST_NAN);

    time.ms = ms;
    return ecc_value_fromfloat(context->thisvalue.data.date->ms = eccdate_msClip(eccdate_msFromDateAndTime(date, time)));
}

static eccvalue_t objdatefn_setSeconds(ecccontext_t* context)
{
    eccdtdate_t date;
    eccdttime_t time;
    double s, ms;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    eccdate_msToDateAndTime(eccdate_toLocal(context->thisvalue.data.date->ms), &date, &time);
    s = eccdate_binaryArgumentOr(context, 0, ECC_CONST_NAN);
    ms = eccdate_binaryArgumentOr(context, 1, time.ms);
    if(isnan(s) || isnan(ms))
        return ecc_value_fromfloat(context->thisvalue.data.date->ms = ECC_CONST_NAN);

    time.s = s;
    time.ms = ms;
    return ecc_value_fromfloat(context->thisvalue.data.date->ms = eccdate_msClip(eccdate_toUTC(eccdate_msFromDateAndTime(date, time))));
}

static eccvalue_t objdatefn_setUTCSeconds(ecccontext_t* context)
{
    eccdtdate_t date;
    eccdttime_t time;
    double s, ms;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    eccdate_msToDateAndTime(context->thisvalue.data.date->ms, &date, &time);
    s = eccdate_binaryArgumentOr(context, 0, ECC_CONST_NAN);
    ms = eccdate_binaryArgumentOr(context, 1, time.ms);
    if(isnan(s) || isnan(ms))
        return ecc_value_fromfloat(context->thisvalue.data.date->ms = ECC_CONST_NAN);

    time.s = s;
    time.ms = ms;
    return ecc_value_fromfloat(context->thisvalue.data.date->ms = eccdate_msClip(eccdate_msFromDateAndTime(date, time)));
}

static eccvalue_t objdatefn_setMinutes(ecccontext_t* context)
{
    eccdtdate_t date;
    eccdttime_t time;
    double m, s, ms;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    eccdate_msToDateAndTime(eccdate_toLocal(context->thisvalue.data.date->ms), &date, &time);
    m = eccdate_binaryArgumentOr(context, 0, ECC_CONST_NAN);
    s = eccdate_binaryArgumentOr(context, 1, time.s);
    ms = eccdate_binaryArgumentOr(context, 2, time.ms);
    if(isnan(m) || isnan(s) || isnan(ms))
        return ecc_value_fromfloat(context->thisvalue.data.date->ms = ECC_CONST_NAN);

    time.m = m;
    time.s = s;
    time.ms = ms;
    return ecc_value_fromfloat(context->thisvalue.data.date->ms = eccdate_msClip(eccdate_toUTC(eccdate_msFromDateAndTime(date, time))));
}

static eccvalue_t objdatefn_setUTCMinutes(ecccontext_t* context)
{
    eccdtdate_t date;
    eccdttime_t time;
    double m, s, ms;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    eccdate_msToDateAndTime(context->thisvalue.data.date->ms, &date, &time);
    m = eccdate_binaryArgumentOr(context, 0, ECC_CONST_NAN);
    s = eccdate_binaryArgumentOr(context, 1, time.s);
    ms = eccdate_binaryArgumentOr(context, 2, time.ms);
    if(isnan(m) || isnan(s) || isnan(ms))
        return ecc_value_fromfloat(context->thisvalue.data.date->ms = ECC_CONST_NAN);

    time.m = m;
    time.s = s;
    time.ms = ms;
    return ecc_value_fromfloat(context->thisvalue.data.date->ms = eccdate_msClip(eccdate_msFromDateAndTime(date, time)));
}

static eccvalue_t objdatefn_setHours(ecccontext_t* context)
{
    eccdtdate_t date;
    eccdttime_t time;
    double h, m, s, ms;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    eccdate_msToDateAndTime(eccdate_toLocal(context->thisvalue.data.date->ms), &date, &time);
    h = eccdate_binaryArgumentOr(context, 0, ECC_CONST_NAN);
    m = eccdate_binaryArgumentOr(context, 1, time.m);
    s = eccdate_binaryArgumentOr(context, 2, time.s);
    ms = eccdate_binaryArgumentOr(context, 3, time.ms);
    if(isnan(h) || isnan(m) || isnan(s) || isnan(ms))
        return ecc_value_fromfloat(context->thisvalue.data.date->ms = ECC_CONST_NAN);

    time.h = h;
    time.m = m;
    time.s = s;
    time.ms = ms;
    return ecc_value_fromfloat(context->thisvalue.data.date->ms = eccdate_msClip(eccdate_toUTC(eccdate_msFromDateAndTime(date, time))));
}

static eccvalue_t objdatefn_setUTCHours(ecccontext_t* context)
{
    eccdtdate_t date;
    eccdttime_t time;
    double h, m, s, ms;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    eccdate_msToDateAndTime(context->thisvalue.data.date->ms, &date, &time);
    h = eccdate_binaryArgumentOr(context, 0, ECC_CONST_NAN);
    m = eccdate_binaryArgumentOr(context, 1, time.m);
    s = eccdate_binaryArgumentOr(context, 2, time.s);
    ms = eccdate_binaryArgumentOr(context, 3, time.ms);
    if(isnan(h) || isnan(m) || isnan(s) || isnan(ms))
        return ecc_value_fromfloat(context->thisvalue.data.date->ms = ECC_CONST_NAN);

    time.h = h;
    time.m = m;
    time.s = s;
    time.ms = ms;
    return ecc_value_fromfloat(context->thisvalue.data.date->ms = eccdate_msClip(eccdate_msFromDateAndTime(date, time)));
}

static eccvalue_t objdatefn_setDate(ecccontext_t* context)
{
    eccdtdate_t date;
    double day, ms;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    ms = eccdate_msToDate(eccdate_toLocal(context->thisvalue.data.date->ms), &date);
    day = eccdate_binaryArgumentOr(context, 0, ECC_CONST_NAN);
    if(isnan(day))
        return ecc_value_fromfloat(context->thisvalue.data.date->ms = ECC_CONST_NAN);

    date.day = day;
    return ecc_value_fromfloat(context->thisvalue.data.date->ms = eccdate_msClip(eccdate_toUTC(ms + eccdate_msFromDate(date))));
}

static eccvalue_t objdatefn_setUTCDate(ecccontext_t* context)
{
    eccdtdate_t date;
    double day, ms;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    ms = eccdate_msToDate(context->thisvalue.data.date->ms, &date);
    day = eccdate_binaryArgumentOr(context, 0, ECC_CONST_NAN);
    if(isnan(day))
        return ecc_value_fromfloat(context->thisvalue.data.date->ms = ECC_CONST_NAN);

    date.day = day;
    return ecc_value_fromfloat(context->thisvalue.data.date->ms = eccdate_msClip(ms + eccdate_msFromDate(date)));
}

static eccvalue_t objdatefn_setMonth(ecccontext_t* context)
{
    eccdtdate_t date;
    double month, day, ms;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    ms = eccdate_msToDate(eccdate_toLocal(context->thisvalue.data.date->ms), &date);
    month = eccdate_binaryArgumentOr(context, 0, ECC_CONST_NAN) + 1;
    day = eccdate_binaryArgumentOr(context, 1, date.day);
    if(isnan(month) || isnan(day))
        return ecc_value_fromfloat(context->thisvalue.data.date->ms = ECC_CONST_NAN);

    date.month = month;
    date.day = day;
    return ecc_value_fromfloat(context->thisvalue.data.date->ms = eccdate_msClip(eccdate_toUTC(ms + eccdate_msFromDate(date))));
}

static eccvalue_t objdatefn_setUTCMonth(ecccontext_t* context)
{
    eccdtdate_t date;
    double month, day, ms;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    ms = eccdate_msToDate(context->thisvalue.data.date->ms, &date);
    month = eccdate_binaryArgumentOr(context, 0, ECC_CONST_NAN) + 1;
    day = eccdate_binaryArgumentOr(context, 1, date.day);
    if(isnan(month) || isnan(day))
        return ecc_value_fromfloat(context->thisvalue.data.date->ms = ECC_CONST_NAN);

    date.month = month;
    date.day = day;
    return ecc_value_fromfloat(context->thisvalue.data.date->ms = eccdate_msClip(ms + eccdate_msFromDate(date)));
}

static eccvalue_t objdatefn_setFullYear(ecccontext_t* context)
{
    eccdtdate_t date;
    double year, month, day, ms;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    if(isnan(context->thisvalue.data.date->ms))
        context->thisvalue.data.date->ms = 0;

    ms = eccdate_msToDate(eccdate_toLocal(context->thisvalue.data.date->ms), &date);
    year = eccdate_binaryArgumentOr(context, 0, ECC_CONST_NAN);
    month = eccdate_binaryArgumentOr(context, 1, date.month - 1) + 1;
    day = eccdate_binaryArgumentOr(context, 2, date.day);
    if(isnan(year) || isnan(month) || isnan(day))
        return ecc_value_fromfloat(context->thisvalue.data.date->ms = ECC_CONST_NAN);

    date.year = year;
    date.month = month;
    date.day = day;
    return ecc_value_fromfloat(context->thisvalue.data.date->ms = eccdate_msClip(eccdate_toUTC(ms + eccdate_msFromDate(date))));
}

static eccvalue_t objdatefn_setYear(ecccontext_t* context)
{
    eccdtdate_t date;
    double year, month, day, ms;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    if(isnan(context->thisvalue.data.date->ms))
        context->thisvalue.data.date->ms = 0;

    ms = eccdate_msToDate(eccdate_toLocal(context->thisvalue.data.date->ms), &date);
    year = eccdate_binaryArgumentOr(context, 0, ECC_CONST_NAN);
    month = eccdate_binaryArgumentOr(context, 1, date.month - 1) + 1;
    day = eccdate_binaryArgumentOr(context, 2, date.day);
    if(isnan(year) || isnan(month) || isnan(day))
        return ecc_value_fromfloat(context->thisvalue.data.date->ms = ECC_CONST_NAN);

    date.year = year < 100 ? year + 1900 : year;
    date.month = month;
    date.day = day;
    return ecc_value_fromfloat(context->thisvalue.data.date->ms = eccdate_msClip(eccdate_toUTC(ms + eccdate_msFromDate(date))));
}

static eccvalue_t objdatefn_setUTCFullYear(ecccontext_t* context)
{
    eccdtdate_t date;
    double year, month, day, ms;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    if(isnan(context->thisvalue.data.date->ms))
        context->thisvalue.data.date->ms = 0;

    ms = eccdate_msToDate(context->thisvalue.data.date->ms, &date);
    year = eccdate_binaryArgumentOr(context, 0, ECC_CONST_NAN);
    month = eccdate_binaryArgumentOr(context, 1, date.month - 1) + 1;
    day = eccdate_binaryArgumentOr(context, 2, date.day);
    if(isnan(year) || isnan(month) || isnan(day))
        return ecc_value_fromfloat(context->thisvalue.data.date->ms = ECC_CONST_NAN);

    date.year = year;
    date.month = month;
    date.day = day;
    return ecc_value_fromfloat(context->thisvalue.data.date->ms = eccdate_msClip(ms + eccdate_msFromDate(date)));
}

static eccvalue_t objdatefn_now(ecccontext_t* context)
{
    (void)context;
    return ecc_value_fromfloat(eccdate_msClip(ecc_env_currenttime()));
}

static eccvalue_t objdatefn_parse(ecccontext_t* context)
{
    eccvalue_t value;

    value = ecc_value_tostring(context, ecc_context_argument(context, 0));

    return ecc_value_fromfloat(eccdate_msClip(eccdate_msFromBytes(ecc_value_stringbytes(&value), ecc_value_stringlength(&value))));
}

static eccvalue_t objdatefn_UTC(ecccontext_t* context)
{
    if(ecc_context_argumentcount(context) > 1)
        return ecc_value_fromfloat(eccdate_msClip(eccdate_msFromArguments(context)));

    return ecc_value_fromfloat(ECC_CONST_NAN);
}

static eccvalue_t objdatefn_constructor(ecccontext_t* context)
{
    double time;
    uint32_t count;

    if(!context->construct)
        return ecc_value_fromchars(eccdate_msToChars(ecc_env_currenttime(), localOffset));

    count = ecc_context_argumentcount(context);

    if(count > 1)
        time = eccdate_toUTC(eccdate_msFromArguments(context));
    else if(count)
    {
        eccvalue_t value = ecc_value_toprimitive(context, ecc_context_argument(context, 0), ECC_VALHINT_AUTO);

        if(ecc_value_isstring(value))
            time = eccdate_msFromBytes(ecc_value_stringbytes(&value), ecc_value_stringlength(&value));
        else
            time = ecc_value_tobinary(context, value).data.valnumfloat;
    }
    else
        time = ecc_env_currenttime();

    return ecc_value_date(ecc_date_create(time));
}

void ecc_date_setup(void)
{
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;

    eccdate_setupLocalOffset();

    ecc_function_setupbuiltinobject(&ECC_CtorFunc_Date, objdatefn_constructor, -7, &ECC_Prototype_Date, ecc_value_date(ecc_date_create(ECC_CONST_NAN)), &ECC_Type_Date);

    ecc_function_addmethod(ECC_CtorFunc_Date, "parse", objdatefn_parse, 1, h);
    ecc_function_addmethod(ECC_CtorFunc_Date, "UTC", objdatefn_UTC, -7, h);
    ecc_function_addmethod(ECC_CtorFunc_Date, "now", objdatefn_now, 0, h);

    ecc_function_addto(ECC_Prototype_Date, "toString", objdatefn_toString, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "toDateString", objdatefn_toDateString, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "toTimeString", objdatefn_toTimeString, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "toLocaleString", objdatefn_toString, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "toLocaleDateString", objdatefn_toDateString, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "toLocaleTimeString", objdatefn_toTimeString, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "valueOf", objdatefn_valueOf, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getTime", objdatefn_valueOf, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getYear", objdatefn_getYear, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getFullYear", objdatefn_getFullYear, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getUTCFullYear", objdatefn_getUTCFullYear, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getMonth", objdatefn_getMonth, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getUTCMonth", objdatefn_getUTCMonth, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getDate", objdatefn_getDate, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getUTCDate", objdatefn_getUTCDate, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getDay", objdatefn_getDay, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getUTCDay", objdatefn_getUTCDay, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getHours", objdatefn_getHours, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getUTCHours", objdatefn_getUTCHours, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getMinutes", objdatefn_getMinutes, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getUTCMinutes", objdatefn_getUTCMinutes, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getSeconds", objdatefn_getSeconds, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getUTCSeconds", objdatefn_getUTCSeconds, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getMilliseconds", objdatefn_getMilliseconds, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getUTCMilliseconds", objdatefn_getUTCMilliseconds, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getTimezoneOffset", objdatefn_getTimezoneOffset, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "setTime", objdatefn_setTime, 1, h);
    ecc_function_addto(ECC_Prototype_Date, "setMilliseconds", objdatefn_setMilliseconds, 1, h);
    ecc_function_addto(ECC_Prototype_Date, "setUTCMilliseconds", objdatefn_setUTCMilliseconds, 1, h);
    ecc_function_addto(ECC_Prototype_Date, "setSeconds", objdatefn_setSeconds, 2, h);
    ecc_function_addto(ECC_Prototype_Date, "setUTCSeconds", objdatefn_setUTCSeconds, 2, h);
    ecc_function_addto(ECC_Prototype_Date, "setMinutes", objdatefn_setMinutes, 3, h);
    ecc_function_addto(ECC_Prototype_Date, "setUTCMinutes", objdatefn_setUTCMinutes, 3, h);
    ecc_function_addto(ECC_Prototype_Date, "setHours", objdatefn_setHours, 4, h);
    ecc_function_addto(ECC_Prototype_Date, "setUTCHours", objdatefn_setUTCHours, 4, h);
    ecc_function_addto(ECC_Prototype_Date, "setDate", objdatefn_setDate, 1, h);
    ecc_function_addto(ECC_Prototype_Date, "setUTCDate", objdatefn_setUTCDate, 1, h);
    ecc_function_addto(ECC_Prototype_Date, "setMonth", objdatefn_setMonth, 2, h);
    ecc_function_addto(ECC_Prototype_Date, "setUTCMonth", objdatefn_setUTCMonth, 2, h);
    ecc_function_addto(ECC_Prototype_Date, "setYear", objdatefn_setYear, 3, h);
    ecc_function_addto(ECC_Prototype_Date, "setFullYear", objdatefn_setFullYear, 3, h);
    ecc_function_addto(ECC_Prototype_Date, "setUTCFullYear", objdatefn_setUTCFullYear, 3, h);
    ecc_function_addto(ECC_Prototype_Date, "toUTCString", objdatefn_toUTCString, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "toGMTString", objdatefn_toUTCString, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "toISOString", objdatefn_toISOString, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "toJSON", objdatefn_toJSON, 1, h);
}

void ecc_date_teardown(void)
{
    ECC_Prototype_Date = NULL;
    ECC_CtorFunc_Date = NULL;
}

eccobjdate_t* ecc_date_create(double ms)
{
    eccobjdate_t* self = (eccobjdate_t*)malloc(sizeof(*self));
    memset(self, 0, sizeof(eccobjdate_t));
    ecc_mempool_addobject(&self->object);
    ecc_object_initialize(&self->object, ECC_Prototype_Date);

    self->ms = eccdate_msClip(ms);

    return self;
}
