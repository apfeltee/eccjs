
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

static int ecc_date_issign(int c);
static void ecc_date_setuplocaloffset(void);
static double ecc_date_tolocal(double ms);
static double ecc_date_toutc(double ms);
static double ecc_date_binaryargumentor(ecccontext_t *context, int index, double alternative);
static double ecc_date_msclip(double ms);
static double ecc_date_msfromdate(eccdtdate_t date);
static double ecc_date_msfromdateandtime(eccdtdate_t date, eccdttime_t time);
static double ecc_date_msfromarguments(ecccontext_t *context);
static double ecc_date_msfrombytes(const char *bytes, uint32_t length);
static double ecc_date_mstodate(double ms, eccdtdate_t *date);
static void ecc_date_mstotime(double ms, eccdttime_t *time);
static void ecc_date_mstodateandtime(double ms, eccdtdate_t *date, eccdttime_t *time);
static double ecc_date_mstohours(double ms);
static double ecc_date_mstominutes(double ms);
static double ecc_date_mstoseconds(double ms);
static double ecc_date_mstomilliseconds(double ms);
static eccstrbuffer_t *ecc_date_mstochars(double ms, double offset);
static unsigned int ecc_date_mstoweekday(double ms);
static eccvalue_t ecc_objfndate_tostring(ecccontext_t *context);
static eccvalue_t ecc_objfndate_toutcstring(ecccontext_t *context);
static eccvalue_t ecc_objfndate_tojson(ecccontext_t *context);
static eccvalue_t ecc_objfndate_toisostring(ecccontext_t *context);
static eccvalue_t ecc_objfndate_todatestring(ecccontext_t *context);
static eccvalue_t ecc_objfndate_totimestring(ecccontext_t *context);
static eccvalue_t ecc_objfndate_valueof(ecccontext_t *context);
static eccvalue_t ecc_objfndate_getyear(ecccontext_t *context);
static eccvalue_t ecc_objfndate_getfullyear(ecccontext_t *context);
static eccvalue_t ecc_objfndate_getutcfullyear(ecccontext_t *context);
static eccvalue_t ecc_objfndate_getmonth(ecccontext_t *context);
static eccvalue_t ecc_objfndate_getutcmonth(ecccontext_t *context);
static eccvalue_t ecc_objfndate_getdate(ecccontext_t *context);
static eccvalue_t ecc_objfndate_getutcdate(ecccontext_t *context);
static eccvalue_t ecc_objfndate_getday(ecccontext_t *context);
static eccvalue_t ecc_objfndate_getutcday(ecccontext_t *context);
static eccvalue_t ecc_objfndate_gethours(ecccontext_t *context);
static eccvalue_t ecc_objfndate_getutchours(ecccontext_t *context);
static eccvalue_t ecc_objfndate_getminutes(ecccontext_t *context);
static eccvalue_t ecc_objfndate_getutcminutes(ecccontext_t *context);
static eccvalue_t ecc_objfndate_getseconds(ecccontext_t *context);
static eccvalue_t ecc_objfndate_getutcseconds(ecccontext_t *context);
static eccvalue_t ecc_objfndate_getmilliseconds(ecccontext_t *context);
static eccvalue_t ecc_objfndate_getutcmilliseconds(ecccontext_t *context);
static eccvalue_t ecc_objfndate_gettimezoneoffset(ecccontext_t *context);
static eccvalue_t ecc_objfndate_settime(ecccontext_t *context);
static eccvalue_t ecc_objfndate_setmilliseconds(ecccontext_t *context);
static eccvalue_t ecc_objfndate_setutcmilliseconds(ecccontext_t *context);
static eccvalue_t ecc_objfndate_setseconds(ecccontext_t *context);
static eccvalue_t ecc_objfndate_setutcseconds(ecccontext_t *context);
static eccvalue_t ecc_objfndate_setminutes(ecccontext_t *context);
static eccvalue_t ecc_objfndate_setutcminutes(ecccontext_t *context);
static eccvalue_t ecc_objfndate_sethours(ecccontext_t *context);
static eccvalue_t ecc_objfndate_setutchours(ecccontext_t *context);
static eccvalue_t ecc_objfndate_setdate(ecccontext_t *context);
static eccvalue_t ecc_objfndate_setutcdate(ecccontext_t *context);
static eccvalue_t ecc_objfndate_setmonth(ecccontext_t *context);
static eccvalue_t ecc_objfndate_setutcmonth(ecccontext_t *context);
static eccvalue_t ecc_objfndate_setfullyear(ecccontext_t *context);
static eccvalue_t ecc_objfndate_setyear(ecccontext_t *context);
static eccvalue_t ecc_objfndate_setutcfullyear(ecccontext_t *context);
static eccvalue_t ecc_objfndate_now(ecccontext_t *context);
static eccvalue_t ecc_objfndate_parse(ecccontext_t *context);
static eccvalue_t ecc_objfndate_utc(ecccontext_t *context);
static eccvalue_t ecc_objfndate_constructor(ecccontext_t *context);

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

static int ecc_date_issign(int c)
{
    return c == '+' || c == '-';
}

static void ecc_date_setuplocaloffset(void)
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

static double ecc_date_tolocal(double ms)
{
    return ms + localOffset * msPerHour;
}

static double ecc_date_toutc(double ms)
{
    return ms - localOffset * msPerHour;
}

static double ecc_date_binaryargumentor(ecccontext_t* context, int index, double alternative)
{
    eccvalue_t value = ecc_context_argument(context, index);
    if(value.check == 1)
        return ecc_value_tobinary(context, ecc_context_argument(context, index)).data.valnumfloat;
    else
        return alternative;
}

static double ecc_date_msclip(double ms)
{
    if(!isfinite(ms) || fabs(ms) > 8.64 * pow(10, 15))
        return ECC_CONST_NAN;
    else
        return ms;
}

static double ecc_date_msfromdate(eccdtdate_t date)
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

static double ecc_date_msfromdateandtime(eccdtdate_t date, eccdttime_t time)
{
    return ecc_date_msfromdate(date) + time.h * msPerHour + time.m * msPerMinute + time.s * msPerSecond + time.ms;
}

static double ecc_date_msfromarguments(ecccontext_t* context)
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
        return ecc_date_msfromdateandtime(date, time);
    }
}

static double ecc_date_msfrombytes(const char* bytes, uint32_t length)
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
                      ecc_date_issign(buffer[0]) ? "%07d"
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
        else if(ecc_date_issign(buffer[n]) && sscanf(buffer + n, "%03d:%02d%n", &hOffset, &mOffset, &nOffset) == 2 && n + nOffset == length)
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
    else if(i >= 5 && buffer[n++] == ' ' && ecc_date_issign(buffer[n]) && sscanf(buffer + n, "%03d%02d%n", &hOffset, &mOffset, &nOffset) == 2 && n + nOffset == length)
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
    return ecc_date_msfromdateandtime(date, time) - (hOffset * 60 + mOffset) * msPerMinute;
}

static double ecc_date_mstodate(double ms, eccdtdate_t* date)
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

static void ecc_date_mstotime(double ms, eccdttime_t* time)
{
    time->h = fmod(floor(ms / msPerHour), 24);
    time->m = fmod(floor(ms / msPerMinute), 60);
    time->s = fmod(floor(ms / msPerSecond), 60);
    time->ms = fmod(ms, 1000);
}

static void ecc_date_mstodateandtime(double ms, eccdtdate_t* date, eccdttime_t* time)
{
    ecc_date_mstotime(ecc_date_mstodate(ms, date), time);
}

static double ecc_date_mstohours(double ms)
{
    eccdtdate_t date;
    eccdttime_t time;
    ecc_date_mstodateandtime(ms, &date, &time);
    return time.h;
}

static double ecc_date_mstominutes(double ms)
{
    eccdtdate_t date;
    eccdttime_t time;
    ecc_date_mstodateandtime(ms, &date, &time);
    return time.m;
}

static double ecc_date_mstoseconds(double ms)
{
    eccdtdate_t date;
    eccdttime_t time;
    ecc_date_mstodateandtime(ms, &date, &time);
    return time.s;
}

static double ecc_date_mstomilliseconds(double ms)
{
    eccdtdate_t date;
    eccdttime_t time;
    ecc_date_mstodateandtime(ms, &date, &time);
    return time.ms;
}

static eccstrbuffer_t* ecc_date_mstochars(double ms, double offset)
{
    const char* format;
    eccdtdate_t date;
    eccdttime_t time;

    if(isnan(ms))
        return ecc_strbuf_create("Invalid Date");

    ecc_date_mstodateandtime(ms + offset * msPerHour, &date, &time);

    if(date.year >= 100 && date.year <= 9999)
        format = "%04d/%02d/%02d %02d:%02d:%02d %+03d%02d";
    else
        format = "%+06d-%02d-%02dT%02d:%02d:%02d%+03d:%02d";

    return ecc_strbuf_create(format, date.year, date.month, date.day, time.h, time.m, time.s, (int)offset, (int)fmod(fabs(offset) * 60, 60));
}

static unsigned int ecc_date_mstoweekday(double ms)
{
    int z = ms / msPerDay;
    return (unsigned)(z >= -4 ? (z + 4) % 7 : (z + 5) % 7 + 6);
}

static eccvalue_t ecc_objfndate_tostring(ecccontext_t* context)
{
    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    return ecc_value_fromchars(ecc_date_mstochars(context->thisvalue.data.date->ms, localOffset));
}

static eccvalue_t ecc_objfndate_toutcstring(ecccontext_t* context)
{
    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    return ecc_value_fromchars(ecc_date_mstochars(context->thisvalue.data.date->ms, 0));
}

static eccvalue_t ecc_objfndate_tojson(ecccontext_t* context)
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

static eccvalue_t ecc_objfndate_toisostring(ecccontext_t* context)
{
    const char* format;
    eccdtdate_t date;
    eccdttime_t time;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    if(isnan(context->thisvalue.data.date->ms))
        ecc_context_rangeerror(context, ecc_strbuf_create("invalid date"));

    ecc_date_mstodateandtime(context->thisvalue.data.date->ms, &date, &time);

    if(date.year >= 0 && date.year <= 9999)
        format = "%04d-%02d-%02dT%02d:%02d:%06.3fZ";
    else
        format = "%+06d-%02d-%02dT%02d:%02d:%06.3fZ";

    return ecc_value_fromchars(ecc_strbuf_create(format, date.year, date.month, date.day, time.h, time.m, time.s + (time.ms / 1000.)));
}

static eccvalue_t ecc_objfndate_todatestring(ecccontext_t* context)
{
    eccdtdate_t date;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    if(isnan(context->thisvalue.data.date->ms))
        return ecc_value_fromchars(ecc_strbuf_create("Invalid Date"));

    ecc_date_mstodate(ecc_date_tolocal(context->thisvalue.data.date->ms), &date);

    return ecc_value_fromchars(ecc_strbuf_create("%04d/%02d/%02d", date.year, date.month, date.day));
}

static eccvalue_t ecc_objfndate_totimestring(ecccontext_t* context)
{
    eccdttime_t time;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    if(isnan(context->thisvalue.data.date->ms))
        return ecc_value_fromchars(ecc_strbuf_create("Invalid Date"));

    ecc_date_mstotime(ecc_date_tolocal(context->thisvalue.data.date->ms), &time);

    return ecc_value_fromchars(ecc_strbuf_create("%02d:%02d:%02d %+03d%02d", time.h, time.m, time.s, (int)localOffset, (int)fmod(fabs(localOffset) * 60, 60)));
}

static eccvalue_t ecc_objfndate_valueof(ecccontext_t* context)
{
    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    return ecc_value_fromfloat(context->thisvalue.data.date->ms >= 0 ? floor(context->thisvalue.data.date->ms) : ceil(context->thisvalue.data.date->ms));
}

static eccvalue_t ecc_objfndate_getyear(ecccontext_t* context)
{
    eccdtdate_t date;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    ecc_date_mstodate(ecc_date_tolocal(context->thisvalue.data.date->ms), &date);
    return ecc_value_fromfloat(date.year - 1900);
}

static eccvalue_t ecc_objfndate_getfullyear(ecccontext_t* context)
{
    eccdtdate_t date;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    ecc_date_mstodate(ecc_date_tolocal(context->thisvalue.data.date->ms), &date);
    return ecc_value_fromfloat(date.year);
}

static eccvalue_t ecc_objfndate_getutcfullyear(ecccontext_t* context)
{
    eccdtdate_t date;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    ecc_date_mstodate(context->thisvalue.data.date->ms, &date);
    return ecc_value_fromfloat(date.year);
}

static eccvalue_t ecc_objfndate_getmonth(ecccontext_t* context)
{
    eccdtdate_t date;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    ecc_date_mstodate(ecc_date_tolocal(context->thisvalue.data.date->ms), &date);
    return ecc_value_fromfloat(date.month - 1);
}

static eccvalue_t ecc_objfndate_getutcmonth(ecccontext_t* context)
{
    eccdtdate_t date;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    ecc_date_mstodate(context->thisvalue.data.date->ms, &date);
    return ecc_value_fromfloat(date.month - 1);
}

static eccvalue_t ecc_objfndate_getdate(ecccontext_t* context)
{
    eccdtdate_t date;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    ecc_date_mstodate(ecc_date_tolocal(context->thisvalue.data.date->ms), &date);
    return ecc_value_fromfloat(date.day);
}

static eccvalue_t ecc_objfndate_getutcdate(ecccontext_t* context)
{
    eccdtdate_t date;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    ecc_date_mstodate(context->thisvalue.data.date->ms, &date);
    return ecc_value_fromfloat(date.day);
}

static eccvalue_t ecc_objfndate_getday(ecccontext_t* context)
{
    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    return ecc_value_fromfloat(ecc_date_mstoweekday(ecc_date_tolocal(context->thisvalue.data.date->ms)));
}

static eccvalue_t ecc_objfndate_getutcday(ecccontext_t* context)
{
    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    return ecc_value_fromfloat(ecc_date_mstoweekday(context->thisvalue.data.date->ms));
}

static eccvalue_t ecc_objfndate_gethours(ecccontext_t* context)
{
    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    return ecc_value_fromfloat(ecc_date_mstohours(ecc_date_tolocal(context->thisvalue.data.date->ms)));
}

static eccvalue_t ecc_objfndate_getutchours(ecccontext_t* context)
{
    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    return ecc_value_fromfloat(ecc_date_mstohours(context->thisvalue.data.date->ms));
}

static eccvalue_t ecc_objfndate_getminutes(ecccontext_t* context)
{
    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    return ecc_value_fromfloat(ecc_date_mstominutes(ecc_date_tolocal(context->thisvalue.data.date->ms)));
}

static eccvalue_t ecc_objfndate_getutcminutes(ecccontext_t* context)
{
    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    return ecc_value_fromfloat(ecc_date_mstominutes(context->thisvalue.data.date->ms));
}

static eccvalue_t ecc_objfndate_getseconds(ecccontext_t* context)
{
    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    return ecc_value_fromfloat(ecc_date_mstoseconds(ecc_date_tolocal(context->thisvalue.data.date->ms)));
}

static eccvalue_t ecc_objfndate_getutcseconds(ecccontext_t* context)
{
    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    return ecc_value_fromfloat(ecc_date_mstoseconds(context->thisvalue.data.date->ms));
}

static eccvalue_t ecc_objfndate_getmilliseconds(ecccontext_t* context)
{
    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    return ecc_value_fromfloat(ecc_date_mstomilliseconds(ecc_date_tolocal(context->thisvalue.data.date->ms)));
}

static eccvalue_t ecc_objfndate_getutcmilliseconds(ecccontext_t* context)
{
    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    return ecc_value_fromfloat(ecc_date_mstomilliseconds(context->thisvalue.data.date->ms));
}

static eccvalue_t ecc_objfndate_gettimezoneoffset(ecccontext_t* context)
{
    (void)context;
    return ecc_value_fromfloat(-localOffset * 60);
}

static eccvalue_t ecc_objfndate_settime(ecccontext_t* context)
{
    double ms;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    ms = ecc_value_tobinary(context, ecc_context_argument(context, 0)).data.valnumfloat;

    return ecc_value_fromfloat(context->thisvalue.data.date->ms = ecc_date_msclip(ms));
}

static eccvalue_t ecc_objfndate_setmilliseconds(ecccontext_t* context)
{
    eccdtdate_t date;
    eccdttime_t time;
    double ms;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    ecc_date_mstodateandtime(ecc_date_tolocal(context->thisvalue.data.date->ms), &date, &time);
    ms = ecc_date_binaryargumentor(context, 0, ECC_CONST_NAN);
    if(isnan(ms))
        return ecc_value_fromfloat(context->thisvalue.data.date->ms = ECC_CONST_NAN);

    time.ms = ms;
    return ecc_value_fromfloat(context->thisvalue.data.date->ms = ecc_date_msclip(ecc_date_toutc(ecc_date_msfromdateandtime(date, time))));
}

static eccvalue_t ecc_objfndate_setutcmilliseconds(ecccontext_t* context)
{
    eccdtdate_t date;
    eccdttime_t time;
    double ms;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    ecc_date_mstodateandtime(context->thisvalue.data.date->ms, &date, &time);
    ms = ecc_date_binaryargumentor(context, 0, ECC_CONST_NAN);
    if(isnan(ms))
        return ecc_value_fromfloat(context->thisvalue.data.date->ms = ECC_CONST_NAN);

    time.ms = ms;
    return ecc_value_fromfloat(context->thisvalue.data.date->ms = ecc_date_msclip(ecc_date_msfromdateandtime(date, time)));
}

static eccvalue_t ecc_objfndate_setseconds(ecccontext_t* context)
{
    eccdtdate_t date;
    eccdttime_t time;
    double s, ms;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    ecc_date_mstodateandtime(ecc_date_tolocal(context->thisvalue.data.date->ms), &date, &time);
    s = ecc_date_binaryargumentor(context, 0, ECC_CONST_NAN);
    ms = ecc_date_binaryargumentor(context, 1, time.ms);
    if(isnan(s) || isnan(ms))
        return ecc_value_fromfloat(context->thisvalue.data.date->ms = ECC_CONST_NAN);

    time.s = s;
    time.ms = ms;
    return ecc_value_fromfloat(context->thisvalue.data.date->ms = ecc_date_msclip(ecc_date_toutc(ecc_date_msfromdateandtime(date, time))));
}

static eccvalue_t ecc_objfndate_setutcseconds(ecccontext_t* context)
{
    eccdtdate_t date;
    eccdttime_t time;
    double s, ms;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    ecc_date_mstodateandtime(context->thisvalue.data.date->ms, &date, &time);
    s = ecc_date_binaryargumentor(context, 0, ECC_CONST_NAN);
    ms = ecc_date_binaryargumentor(context, 1, time.ms);
    if(isnan(s) || isnan(ms))
        return ecc_value_fromfloat(context->thisvalue.data.date->ms = ECC_CONST_NAN);

    time.s = s;
    time.ms = ms;
    return ecc_value_fromfloat(context->thisvalue.data.date->ms = ecc_date_msclip(ecc_date_msfromdateandtime(date, time)));
}

static eccvalue_t ecc_objfndate_setminutes(ecccontext_t* context)
{
    eccdtdate_t date;
    eccdttime_t time;
    double m, s, ms;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    ecc_date_mstodateandtime(ecc_date_tolocal(context->thisvalue.data.date->ms), &date, &time);
    m = ecc_date_binaryargumentor(context, 0, ECC_CONST_NAN);
    s = ecc_date_binaryargumentor(context, 1, time.s);
    ms = ecc_date_binaryargumentor(context, 2, time.ms);
    if(isnan(m) || isnan(s) || isnan(ms))
        return ecc_value_fromfloat(context->thisvalue.data.date->ms = ECC_CONST_NAN);

    time.m = m;
    time.s = s;
    time.ms = ms;
    return ecc_value_fromfloat(context->thisvalue.data.date->ms = ecc_date_msclip(ecc_date_toutc(ecc_date_msfromdateandtime(date, time))));
}

static eccvalue_t ecc_objfndate_setutcminutes(ecccontext_t* context)
{
    eccdtdate_t date;
    eccdttime_t time;
    double m, s, ms;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    ecc_date_mstodateandtime(context->thisvalue.data.date->ms, &date, &time);
    m = ecc_date_binaryargumentor(context, 0, ECC_CONST_NAN);
    s = ecc_date_binaryargumentor(context, 1, time.s);
    ms = ecc_date_binaryargumentor(context, 2, time.ms);
    if(isnan(m) || isnan(s) || isnan(ms))
        return ecc_value_fromfloat(context->thisvalue.data.date->ms = ECC_CONST_NAN);

    time.m = m;
    time.s = s;
    time.ms = ms;
    return ecc_value_fromfloat(context->thisvalue.data.date->ms = ecc_date_msclip(ecc_date_msfromdateandtime(date, time)));
}

static eccvalue_t ecc_objfndate_sethours(ecccontext_t* context)
{
    eccdtdate_t date;
    eccdttime_t time;
    double h, m, s, ms;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    ecc_date_mstodateandtime(ecc_date_tolocal(context->thisvalue.data.date->ms), &date, &time);
    h = ecc_date_binaryargumentor(context, 0, ECC_CONST_NAN);
    m = ecc_date_binaryargumentor(context, 1, time.m);
    s = ecc_date_binaryargumentor(context, 2, time.s);
    ms = ecc_date_binaryargumentor(context, 3, time.ms);
    if(isnan(h) || isnan(m) || isnan(s) || isnan(ms))
        return ecc_value_fromfloat(context->thisvalue.data.date->ms = ECC_CONST_NAN);

    time.h = h;
    time.m = m;
    time.s = s;
    time.ms = ms;
    return ecc_value_fromfloat(context->thisvalue.data.date->ms = ecc_date_msclip(ecc_date_toutc(ecc_date_msfromdateandtime(date, time))));
}

static eccvalue_t ecc_objfndate_setutchours(ecccontext_t* context)
{
    eccdtdate_t date;
    eccdttime_t time;
    double h, m, s, ms;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    ecc_date_mstodateandtime(context->thisvalue.data.date->ms, &date, &time);
    h = ecc_date_binaryargumentor(context, 0, ECC_CONST_NAN);
    m = ecc_date_binaryargumentor(context, 1, time.m);
    s = ecc_date_binaryargumentor(context, 2, time.s);
    ms = ecc_date_binaryargumentor(context, 3, time.ms);
    if(isnan(h) || isnan(m) || isnan(s) || isnan(ms))
        return ecc_value_fromfloat(context->thisvalue.data.date->ms = ECC_CONST_NAN);

    time.h = h;
    time.m = m;
    time.s = s;
    time.ms = ms;
    return ecc_value_fromfloat(context->thisvalue.data.date->ms = ecc_date_msclip(ecc_date_msfromdateandtime(date, time)));
}

static eccvalue_t ecc_objfndate_setdate(ecccontext_t* context)
{
    eccdtdate_t date;
    double day, ms;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    ms = ecc_date_mstodate(ecc_date_tolocal(context->thisvalue.data.date->ms), &date);
    day = ecc_date_binaryargumentor(context, 0, ECC_CONST_NAN);
    if(isnan(day))
        return ecc_value_fromfloat(context->thisvalue.data.date->ms = ECC_CONST_NAN);

    date.day = day;
    return ecc_value_fromfloat(context->thisvalue.data.date->ms = ecc_date_msclip(ecc_date_toutc(ms + ecc_date_msfromdate(date))));
}

static eccvalue_t ecc_objfndate_setutcdate(ecccontext_t* context)
{
    eccdtdate_t date;
    double day, ms;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    ms = ecc_date_mstodate(context->thisvalue.data.date->ms, &date);
    day = ecc_date_binaryargumentor(context, 0, ECC_CONST_NAN);
    if(isnan(day))
        return ecc_value_fromfloat(context->thisvalue.data.date->ms = ECC_CONST_NAN);

    date.day = day;
    return ecc_value_fromfloat(context->thisvalue.data.date->ms = ecc_date_msclip(ms + ecc_date_msfromdate(date)));
}

static eccvalue_t ecc_objfndate_setmonth(ecccontext_t* context)
{
    eccdtdate_t date;
    double month, day, ms;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    ms = ecc_date_mstodate(ecc_date_tolocal(context->thisvalue.data.date->ms), &date);
    month = ecc_date_binaryargumentor(context, 0, ECC_CONST_NAN) + 1;
    day = ecc_date_binaryargumentor(context, 1, date.day);
    if(isnan(month) || isnan(day))
        return ecc_value_fromfloat(context->thisvalue.data.date->ms = ECC_CONST_NAN);

    date.month = month;
    date.day = day;
    return ecc_value_fromfloat(context->thisvalue.data.date->ms = ecc_date_msclip(ecc_date_toutc(ms + ecc_date_msfromdate(date))));
}

static eccvalue_t ecc_objfndate_setutcmonth(ecccontext_t* context)
{
    eccdtdate_t date;
    double month, day, ms;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    ms = ecc_date_mstodate(context->thisvalue.data.date->ms, &date);
    month = ecc_date_binaryargumentor(context, 0, ECC_CONST_NAN) + 1;
    day = ecc_date_binaryargumentor(context, 1, date.day);
    if(isnan(month) || isnan(day))
        return ecc_value_fromfloat(context->thisvalue.data.date->ms = ECC_CONST_NAN);

    date.month = month;
    date.day = day;
    return ecc_value_fromfloat(context->thisvalue.data.date->ms = ecc_date_msclip(ms + ecc_date_msfromdate(date)));
}

static eccvalue_t ecc_objfndate_setfullyear(ecccontext_t* context)
{
    eccdtdate_t date;
    double year, month, day, ms;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    if(isnan(context->thisvalue.data.date->ms))
        context->thisvalue.data.date->ms = 0;

    ms = ecc_date_mstodate(ecc_date_tolocal(context->thisvalue.data.date->ms), &date);
    year = ecc_date_binaryargumentor(context, 0, ECC_CONST_NAN);
    month = ecc_date_binaryargumentor(context, 1, date.month - 1) + 1;
    day = ecc_date_binaryargumentor(context, 2, date.day);
    if(isnan(year) || isnan(month) || isnan(day))
        return ecc_value_fromfloat(context->thisvalue.data.date->ms = ECC_CONST_NAN);

    date.year = year;
    date.month = month;
    date.day = day;
    return ecc_value_fromfloat(context->thisvalue.data.date->ms = ecc_date_msclip(ecc_date_toutc(ms + ecc_date_msfromdate(date))));
}

static eccvalue_t ecc_objfndate_setyear(ecccontext_t* context)
{
    eccdtdate_t date;
    double year, month, day, ms;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    if(isnan(context->thisvalue.data.date->ms))
        context->thisvalue.data.date->ms = 0;

    ms = ecc_date_mstodate(ecc_date_tolocal(context->thisvalue.data.date->ms), &date);
    year = ecc_date_binaryargumentor(context, 0, ECC_CONST_NAN);
    month = ecc_date_binaryargumentor(context, 1, date.month - 1) + 1;
    day = ecc_date_binaryargumentor(context, 2, date.day);
    if(isnan(year) || isnan(month) || isnan(day))
        return ecc_value_fromfloat(context->thisvalue.data.date->ms = ECC_CONST_NAN);

    date.year = year < 100 ? year + 1900 : year;
    date.month = month;
    date.day = day;
    return ecc_value_fromfloat(context->thisvalue.data.date->ms = ecc_date_msclip(ecc_date_toutc(ms + ecc_date_msfromdate(date))));
}

static eccvalue_t ecc_objfndate_setutcfullyear(ecccontext_t* context)
{
    eccdtdate_t date;
    double year, month, day, ms;

    ecc_context_assertthistype(context, ECC_VALTYPE_DATE);

    if(isnan(context->thisvalue.data.date->ms))
        context->thisvalue.data.date->ms = 0;

    ms = ecc_date_mstodate(context->thisvalue.data.date->ms, &date);
    year = ecc_date_binaryargumentor(context, 0, ECC_CONST_NAN);
    month = ecc_date_binaryargumentor(context, 1, date.month - 1) + 1;
    day = ecc_date_binaryargumentor(context, 2, date.day);
    if(isnan(year) || isnan(month) || isnan(day))
        return ecc_value_fromfloat(context->thisvalue.data.date->ms = ECC_CONST_NAN);

    date.year = year;
    date.month = month;
    date.day = day;
    return ecc_value_fromfloat(context->thisvalue.data.date->ms = ecc_date_msclip(ms + ecc_date_msfromdate(date)));
}

static eccvalue_t ecc_objfndate_now(ecccontext_t* context)
{
    (void)context;
    return ecc_value_fromfloat(ecc_date_msclip(ecc_env_currenttime()));
}

static eccvalue_t ecc_objfndate_parse(ecccontext_t* context)
{
    eccvalue_t value;

    value = ecc_value_tostring(context, ecc_context_argument(context, 0));

    return ecc_value_fromfloat(ecc_date_msclip(ecc_date_msfrombytes(ecc_value_stringbytes(&value), ecc_value_stringlength(&value))));
}

static eccvalue_t ecc_objfndate_utc(ecccontext_t* context)
{
    if(ecc_context_argumentcount(context) > 1)
        return ecc_value_fromfloat(ecc_date_msclip(ecc_date_msfromarguments(context)));

    return ecc_value_fromfloat(ECC_CONST_NAN);
}

static eccvalue_t ecc_objfndate_constructor(ecccontext_t* context)
{
    double time;
    uint32_t count;

    if(!context->construct)
        return ecc_value_fromchars(ecc_date_mstochars(ecc_env_currenttime(), localOffset));

    count = ecc_context_argumentcount(context);

    if(count > 1)
        time = ecc_date_toutc(ecc_date_msfromarguments(context));
    else if(count)
    {
        eccvalue_t value = ecc_value_toprimitive(context, ecc_context_argument(context, 0), ECC_VALHINT_AUTO);

        if(ecc_value_isstring(value))
            time = ecc_date_msfrombytes(ecc_value_stringbytes(&value), ecc_value_stringlength(&value));
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

    ecc_date_setuplocaloffset();

    ecc_function_setupbuiltinobject(&ECC_CtorFunc_Date, ecc_objfndate_constructor, -7, &ECC_Prototype_Date, ecc_value_date(ecc_date_create(ECC_CONST_NAN)), &ECC_Type_Date);

    ecc_function_addmethod(ECC_CtorFunc_Date, "parse", ecc_objfndate_parse, 1, h);
    ecc_function_addmethod(ECC_CtorFunc_Date, "UTC", ecc_objfndate_utc, -7, h);
    ecc_function_addmethod(ECC_CtorFunc_Date, "now", ecc_objfndate_now, 0, h);

    ecc_function_addto(ECC_Prototype_Date, "toString", ecc_objfndate_tostring, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "toDateString", ecc_objfndate_todatestring, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "toTimeString", ecc_objfndate_totimestring, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "toLocaleString", ecc_objfndate_tostring, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "toLocaleDateString", ecc_objfndate_todatestring, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "toLocaleTimeString", ecc_objfndate_totimestring, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "valueOf", ecc_objfndate_valueof, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getTime", ecc_objfndate_valueof, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getYear", ecc_objfndate_getyear, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getFullYear", ecc_objfndate_getfullyear, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getUTCFullYear", ecc_objfndate_getutcfullyear, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getMonth", ecc_objfndate_getmonth, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getUTCMonth", ecc_objfndate_getutcmonth, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getDate", ecc_objfndate_getdate, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getUTCDate", ecc_objfndate_getutcdate, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getDay", ecc_objfndate_getday, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getUTCDay", ecc_objfndate_getutcday, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getHours", ecc_objfndate_gethours, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getUTCHours", ecc_objfndate_getutchours, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getMinutes", ecc_objfndate_getminutes, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getUTCMinutes", ecc_objfndate_getutcminutes, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getSeconds", ecc_objfndate_getseconds, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getUTCSeconds", ecc_objfndate_getutcseconds, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getMilliseconds", ecc_objfndate_getmilliseconds, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getUTCMilliseconds", ecc_objfndate_getutcmilliseconds, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "getTimezoneOffset", ecc_objfndate_gettimezoneoffset, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "setTime", ecc_objfndate_settime, 1, h);
    ecc_function_addto(ECC_Prototype_Date, "setMilliseconds", ecc_objfndate_setmilliseconds, 1, h);
    ecc_function_addto(ECC_Prototype_Date, "setUTCMilliseconds", ecc_objfndate_setutcmilliseconds, 1, h);
    ecc_function_addto(ECC_Prototype_Date, "setSeconds", ecc_objfndate_setseconds, 2, h);
    ecc_function_addto(ECC_Prototype_Date, "setUTCSeconds", ecc_objfndate_setutcseconds, 2, h);
    ecc_function_addto(ECC_Prototype_Date, "setMinutes", ecc_objfndate_setminutes, 3, h);
    ecc_function_addto(ECC_Prototype_Date, "setUTCMinutes", ecc_objfndate_setutcminutes, 3, h);
    ecc_function_addto(ECC_Prototype_Date, "setHours", ecc_objfndate_sethours, 4, h);
    ecc_function_addto(ECC_Prototype_Date, "setUTCHours", ecc_objfndate_setutchours, 4, h);
    ecc_function_addto(ECC_Prototype_Date, "setDate", ecc_objfndate_setdate, 1, h);
    ecc_function_addto(ECC_Prototype_Date, "setUTCDate", ecc_objfndate_setutcdate, 1, h);
    ecc_function_addto(ECC_Prototype_Date, "setMonth", ecc_objfndate_setmonth, 2, h);
    ecc_function_addto(ECC_Prototype_Date, "setUTCMonth", ecc_objfndate_setutcmonth, 2, h);
    ecc_function_addto(ECC_Prototype_Date, "setYear", ecc_objfndate_setyear, 3, h);
    ecc_function_addto(ECC_Prototype_Date, "setFullYear", ecc_objfndate_setfullyear, 3, h);
    ecc_function_addto(ECC_Prototype_Date, "setUTCFullYear", ecc_objfndate_setutcfullyear, 3, h);
    ecc_function_addto(ECC_Prototype_Date, "toUTCString", ecc_objfndate_toutcstring, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "toGMTString", ecc_objfndate_toutcstring, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "toISOString", ecc_objfndate_toisostring, 0, h);
    ecc_function_addto(ECC_Prototype_Date, "toJSON", ecc_objfndate_tojson, 1, h);
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

    self->ms = ecc_date_msclip(ms);

    return self;
}
