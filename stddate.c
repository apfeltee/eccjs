//
//  date.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//

#define Implementation
#include "builtins.h"

#include "ecc.h"
#include "pool.h"
#include "env.h"

// MARK: - Private

struct io_libecc_Object * io_libecc_date_prototype = NULL;
struct io_libecc_Function * io_libecc_date_constructor = NULL;

const struct io_libecc_object_Type io_libecc_date_type = {
	.text = &io_libecc_text_dateType,
};

struct date {
	int32_t year;
	int32_t month;
	int32_t day;
};

struct time {
	int32_t h;
	int32_t m;
	int32_t s;
	int32_t ms;
};

static double localOffset;

static const double msPerSecond = 1000;
static const double msPerMinute = 60000;
static const double msPerHour = 3600000;
static const double msPerDay = 86400000;

static void setup(void);
static void teardown(void);
static struct io_libecc_Date* create(double);
const struct type_io_libecc_Date io_libecc_Date = {
    setup,
    teardown,
    create,
};

static
int issign(int c)
{
	return c == '+' || c == '-';
}

static
void setupLocalOffset (void)
{
	struct tm tm = {
		.tm_mday = 2,
		.tm_year = 70,
		.tm_wday = 5,
		.tm_isdst = -1,
	};
	time_t time = mktime(&tm);
	if (time < 0)
		localOffset = -0.;
	else
		localOffset = difftime(86400, time) / 3600.;
}

static
double toLocal (double ms)
{
	return ms + localOffset * msPerHour;
}

static
double toUTC (double ms)
{
	return ms - localOffset * msPerHour;
}

static
double binaryArgumentOr (struct io_libecc_Context * const context, int index, double alternative)
{
	struct io_libecc_Value value = io_libecc_Context.argument(context, index);
	if (value.check == 1)
		return io_libecc_Value.toBinary(context, io_libecc_Context.argument(context, index)).data.binary;
	else
		return alternative;
}

static
double msClip (double ms)
{
	if (!isfinite(ms) || fabs(ms) > 8.64 * pow(10, 15))
		return NAN;
	else
		return ms;
}

static
double msFromDate (struct date date)
{
	// Low-Level io_libecc_Date Algorithms, http://howardhinnant.github.io/date_algorithms.html#days_from_civil
	
	int32_t era;
	uint32_t yoe, doy, doe;
	
	date.year -= date.month <= 2;
    era = (int32_t)((date.year >= 0 ? date.year : date.year - 399) / 400);
    yoe = (uint32_t)(date.year - era * 400);                                     // [0, 399]
    doy = (153 * (date.month + (date.month > 2? -3: 9)) + 2) / 5 + date.day - 1; // [0, 365]
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;                                 // [0, 146096]
	
    return (double)(era * 146097 + (int32_t)(doe) - 719468) * msPerDay;
}

static
double msFromDateAndTime (struct date date, struct time time)
{
	return msFromDate(date)
		+ time.h * msPerHour
		+ time.m * msPerMinute
		+ time.s * msPerSecond
		+ time.ms
		;
}

static
double msFromArguments (struct io_libecc_Context * const context)
{
	uint16_t count;
	struct date date;
	struct time time;
	double year, month, day, h, m, s, ms;
	
	count = io_libecc_Context.argumentCount(context);
	
	year = io_libecc_Value.toBinary(context, io_libecc_Context.argument(context, 0)).data.binary,
	month = io_libecc_Value.toBinary(context, io_libecc_Context.argument(context, 1)).data.binary,
	day = count > 2? io_libecc_Value.toBinary(context, io_libecc_Context.argument(context, 2)).data.binary: 1,
	h = count > 3? io_libecc_Value.toBinary(context, io_libecc_Context.argument(context, 3)).data.binary: 0,
	m = count > 4? io_libecc_Value.toBinary(context, io_libecc_Context.argument(context, 4)).data.binary: 0,
	s = count > 5? io_libecc_Value.toBinary(context, io_libecc_Context.argument(context, 5)).data.binary: 0,
	ms = count > 6? io_libecc_Value.toBinary(context, io_libecc_Context.argument(context, 6)).data.binary: 0;
	
	if (isnan(year) || isnan(month) || isnan(day) || isnan(h) || isnan(m) || isnan(s) || isnan(ms))
		return NAN;
	else
	{
		if (year >= 0 && year <= 99)
			year += 1900;
		
		date.year = year;
		date.month = month + 1;
		date.day = day;
		time.h = h;
		time.m = m;
		time.s = s;
		time.ms = ms;
		return msFromDateAndTime(date, time);
	}
}

static
double msFromBytes (const char *bytes, uint16_t length)
{
	char buffer[length + 1];
	int n = 0, nOffset = 0, i = 0;
	int year, month = 1, day = 1, h = 0, m = 0, s = 0, ms = 0, hOffset = 0, mOffset = 0;
	struct date date;
	struct time time;
	
	if (!length)
		return NAN;
	
	memcpy(buffer, bytes, length);
	buffer[length] = '\0';
	
	n = 0, i = sscanf(buffer, issign(buffer[0])
	                          ? "%07d""%n""-%02d""%n""-%02d%n""T%02d:%02d%n"":%02d%n"".%03d%n"
	                          : "%04d""%n""-%02d""%n""-%02d%n""T%02d:%02d%n"":%02d%n"".%03d%n",
	                             &year,&n, &month,&n, &day,&n, &h,  &m,  &n, &s,  &n, &ms, &n);
	
	if (i <= 3)
	{
		if (n == length)
			goto done;
		
		/*vvv*/
	}
	else if (i >= 5)
	{
		if (buffer[n] == 'Z' && n + 1 == length)
			goto done;
		else if (issign(buffer[n]) && sscanf(buffer + n, "%03d:%02d%n", &hOffset, &mOffset, &nOffset) == 2 && n + nOffset == length)
			goto done;
		else
			return NAN;
	}
	else
		return NAN;
	
	hOffset = localOffset;
	n = 0, i = sscanf(buffer, "%d"   "/%02d" "/%02d%n"" %02d:%02d%n"":%02d%n",
	                           &year, &month, &day,&n,  &h,  &m, &n, &s,  &n);
	
	if (year < 100)
		return NAN;
	
	if (n == length)
		goto done;
	else if (i >= 5 && buffer[n++] == ' ' && issign(buffer[n]) && sscanf(buffer + n, "%03d%02d%n", &hOffset, &mOffset, &nOffset) == 2 && n + nOffset == length)
		goto done;
	else
		return NAN;
	
done:
	if (month <= 0 || day <= 0 || h < 0 || m < 0 || s < 0 || ms < 0 || hOffset < -12 || mOffset < 0)
		return NAN;
	
	if (month > 12 || day > 31 || h > 23 || m > 59 || s > 59 || ms > 999 || hOffset > 14 || mOffset > 59)
		return NAN;
	
	date.year = year;
	date.month = month;
	date.day = day;
	time.h = h;
	time.m = m;
	time.s = s;
	time.ms = ms;
	return msFromDateAndTime(date, time) - (hOffset * 60 + mOffset) * msPerMinute;
}

static
double msToDate (double ms, struct date *date)
{
	// Low-Level io_libecc_Date Algorithms, http://howardhinnant.github.io/date_algorithms.html#civil_from_days
	
	int32_t z, era;
	uint32_t doe, yoe, doy, mp;
	
	z = ms / msPerDay + 719468;
	era = (z >= 0 ? z : z - 146096) / 146097;
    doe = (z - era * 146097);                                     // [0, 146096]
    yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;  // [0, 399]
    doy = doe - (365 * yoe + yoe / 4 - yoe / 100);                // [0, 365]
    mp = (5 * doy + 2) / 153;                                     // [0, 11]
    date->day = doy - (153 * mp + 2) / 5 + 1;                     // [1, 31]
    date->month = mp + (mp < 10? 3: -9);                          // [1, 12]
    date->year = (int32_t)(yoe) + era * 400 + (mp >= 10);
	
	return fmod(ms, 24 * msPerHour) + (ms < 0? 24 * msPerHour: 0);
}

static
void msToTime (double ms, struct time *time)
{
	time->h = fmod(floor(ms / msPerHour), 24);
	time->m = fmod(floor(ms / msPerMinute), 60);
	time->s = fmod(floor(ms / msPerSecond), 60);
	time->ms = fmod(ms, 1000);
}

static
void msToDateAndTime (double ms, struct date *date, struct time *time)
{
	msToTime(msToDate(ms, date), time);
}

static
double msToHours (double ms)
{
	struct date date;
	struct time time;
	msToDateAndTime(ms, &date, &time);
	return time.h;
}

static
double msToMinutes (double ms)
{
	struct date date;
	struct time time;
	msToDateAndTime(ms, &date, &time);
	return time.m;
}

static
double msToSeconds (double ms)
{
	struct date date;
	struct time time;
	msToDateAndTime(ms, &date, &time);
	return time.s;
}

static
double msToMilliseconds (double ms)
{
	struct date date;
	struct time time;
	msToDateAndTime(ms, &date, &time);
	return time.ms;
}

static
struct io_libecc_Chars *msToChars (double ms, double offset)
{
	const char *format;
	struct date date;
	struct time time;
	
	if (isnan(ms))
		return io_libecc_Chars.create("Invalid io_libecc_Date");
	
	msToDateAndTime(ms + offset * msPerHour, &date, &time);
	
	if (date.year >= 100 && date.year <= 9999)
		format = "%04d/%02d/%02d %02d:%02d:%02d %+03d%02d";
	else
		format = "%+06d-%02d-%02dT%02d:%02d:%02d%+03d:%02d";
	
	return io_libecc_Chars.create(format
		, date.year
		, date.month
		, date.day
		, time.h
		, time.m
		, time.s
		, (int)offset
		, (int)fmod(fabs(offset) * 60, 60)
		);
}

static
unsigned int msToWeekday(double ms)
{
	int z = ms / msPerDay;
    return (unsigned)(z >= -4? (z + 4) % 7: (z + 5) % 7 + 6);
}

// MARK: - Static Members

static
struct io_libecc_Value toString (struct io_libecc_Context * const context)
{
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	return io_libecc_Value.chars(msToChars(context->this.data.date->ms, localOffset));
}

static
struct io_libecc_Value toUTCString (struct io_libecc_Context * const context)
{
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	return io_libecc_Value.chars(msToChars(context->this.data.date->ms, 0));
}

static
struct io_libecc_Value toJSON (struct io_libecc_Context * const context)
{
	struct io_libecc_Value object = io_libecc_Value.toObject(context, io_libecc_Context.this(context));
	struct io_libecc_Value tv = io_libecc_Value.toPrimitive(context, object, io_libecc_value_hintNumber);
	struct io_libecc_Value toISO;
	
	if (tv.type == io_libecc_value_binaryType && !isfinite(tv.data.binary))
		return io_libecc_value_null;
	
	toISO = io_libecc_Object.getMember(context, object.data.object, io_libecc_key_toISOString);
	if (toISO.type != io_libecc_value_functionType)
	{
		io_libecc_Context.setTextIndex(context, io_libecc_context_callIndex);
		io_libecc_Context.typeError(context, io_libecc_Chars.create("toISOString is not a function"));
	}
	return io_libecc_Context.callFunction(context, toISO.data.function, object, 0);
}

static
struct io_libecc_Value toISOString (struct io_libecc_Context * const context)
{
	const char *format;
	struct date date;
	struct time time;
	
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	if (isnan(context->this.data.date->ms))
		io_libecc_Context.rangeError(context, io_libecc_Chars.create("invalid date"));
	
	msToDateAndTime(context->this.data.date->ms, &date, &time);
	
	if (date.year >= 0 && date.year <= 9999)
		format = "%04d-%02d-%02dT%02d:%02d:%06.3fZ";
	else
		format = "%+06d-%02d-%02dT%02d:%02d:%06.3fZ";
	
	return io_libecc_Value.chars(io_libecc_Chars.create(format
		, date.year
		, date.month
		, date.day
		, time.h
		, time.m
		, time.s + (time.ms / 1000.)
		));
}

static
struct io_libecc_Value toDateString (struct io_libecc_Context * const context)
{
	struct date date;
	
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	if (isnan(context->this.data.date->ms))
		return io_libecc_Value.chars(io_libecc_Chars.create("Invalid io_libecc_Date"));
	
	msToDate(toLocal(context->this.data.date->ms), &date);
	
	return io_libecc_Value.chars(io_libecc_Chars.create("%04d/%02d/%02d"
		, date.year
		, date.month
		, date.day
		));
}

static
struct io_libecc_Value toTimeString (struct io_libecc_Context * const context)
{
	struct time time;
	
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	if (isnan(context->this.data.date->ms))
		return io_libecc_Value.chars(io_libecc_Chars.create("Invalid io_libecc_Date"));
	
	msToTime(toLocal(context->this.data.date->ms), &time);
	
	return io_libecc_Value.chars(io_libecc_Chars.create("%02d:%02d:%02d %+03d%02d"
		, time.h
		, time.m
		, time.s
		, (int)localOffset
		, (int)fmod(fabs(localOffset) * 60, 60)
		));
}

static
struct io_libecc_Value valueOf (struct io_libecc_Context * const context)
{
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	return io_libecc_Value.binary(context->this.data.date->ms >= 0? floor(context->this.data.date->ms): ceil(context->this.data.date->ms));
}

static
struct io_libecc_Value getYear (struct io_libecc_Context * const context)
{
	struct date date;
	
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	msToDate(toLocal(context->this.data.date->ms), &date);
	return io_libecc_Value.binary(date.year - 1900);
}

static
struct io_libecc_Value getFullYear (struct io_libecc_Context * const context)
{
	struct date date;
	
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	msToDate(toLocal(context->this.data.date->ms), &date);
	return io_libecc_Value.binary(date.year);
}

static
struct io_libecc_Value getUTCFullYear (struct io_libecc_Context * const context)
{
	struct date date;
	
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	msToDate(context->this.data.date->ms, &date);
	return io_libecc_Value.binary(date.year);
}

static
struct io_libecc_Value getMonth (struct io_libecc_Context * const context)
{
	struct date date;
	
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	msToDate(toLocal(context->this.data.date->ms), &date);
	return io_libecc_Value.binary(date.month - 1);
}

static
struct io_libecc_Value getUTCMonth (struct io_libecc_Context * const context)
{
	struct date date;
	
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	msToDate(context->this.data.date->ms, &date);
	return io_libecc_Value.binary(date.month - 1);
}

static
struct io_libecc_Value getDate (struct io_libecc_Context * const context)
{
	struct date date;
	
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	msToDate(toLocal(context->this.data.date->ms), &date);
	return io_libecc_Value.binary(date.day);
}

static
struct io_libecc_Value getUTCDate (struct io_libecc_Context * const context)
{
	struct date date;
	
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	msToDate(context->this.data.date->ms, &date);
	return io_libecc_Value.binary(date.day);
}

static
struct io_libecc_Value getDay (struct io_libecc_Context * const context)
{
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	return io_libecc_Value.binary(msToWeekday(toLocal(context->this.data.date->ms)));
}

static
struct io_libecc_Value getUTCDay (struct io_libecc_Context * const context)
{
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	return io_libecc_Value.binary(msToWeekday(context->this.data.date->ms));
}

static
struct io_libecc_Value getHours (struct io_libecc_Context * const context)
{
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	return io_libecc_Value.binary(msToHours(toLocal(context->this.data.date->ms)));
}

static
struct io_libecc_Value getUTCHours (struct io_libecc_Context * const context)
{
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	return io_libecc_Value.binary(msToHours(context->this.data.date->ms));
}

static
struct io_libecc_Value getMinutes (struct io_libecc_Context * const context)
{
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	return io_libecc_Value.binary(msToMinutes(toLocal(context->this.data.date->ms)));
}

static
struct io_libecc_Value getUTCMinutes (struct io_libecc_Context * const context)
{
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	return io_libecc_Value.binary(msToMinutes(context->this.data.date->ms));
}

static
struct io_libecc_Value getSeconds (struct io_libecc_Context * const context)
{
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	return io_libecc_Value.binary(msToSeconds(toLocal(context->this.data.date->ms)));
}

static
struct io_libecc_Value getUTCSeconds (struct io_libecc_Context * const context)
{
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	return io_libecc_Value.binary(msToSeconds(context->this.data.date->ms));
}

static
struct io_libecc_Value getMilliseconds (struct io_libecc_Context * const context)
{
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	return io_libecc_Value.binary(msToMilliseconds(toLocal(context->this.data.date->ms)));
}

static
struct io_libecc_Value getUTCMilliseconds (struct io_libecc_Context * const context)
{
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	return io_libecc_Value.binary(msToMilliseconds(context->this.data.date->ms));
}

static
struct io_libecc_Value getTimezoneOffset (struct io_libecc_Context * const context)
{
	return io_libecc_Value.binary(-localOffset * 60);
}

static
struct io_libecc_Value setTime (struct io_libecc_Context * const context)
{
	double ms;
	
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	ms = io_libecc_Value.toBinary(context, io_libecc_Context.argument(context, 0)).data.binary;
	
	return io_libecc_Value.binary(context->this.data.date->ms = msClip(ms));
}

static
struct io_libecc_Value setMilliseconds (struct io_libecc_Context * const context)
{
	struct date date;
	struct time time;
	double ms;
	
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	msToDateAndTime(toLocal(context->this.data.date->ms), &date, &time);
	ms = binaryArgumentOr(context, 0, NAN);
	if (isnan(ms))
		return io_libecc_Value.binary(context->this.data.date->ms = NAN);
	
	time.ms = ms;
	return io_libecc_Value.binary(context->this.data.date->ms = msClip(toUTC(msFromDateAndTime(date, time))));
}

static
struct io_libecc_Value setUTCMilliseconds (struct io_libecc_Context * const context)
{
	struct date date;
	struct time time;
	double ms;
	
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	msToDateAndTime(context->this.data.date->ms, &date, &time);
	ms = binaryArgumentOr(context, 0, NAN);
	if (isnan(ms))
		return io_libecc_Value.binary(context->this.data.date->ms = NAN);
	
	time.ms = ms;
	return io_libecc_Value.binary(context->this.data.date->ms = msClip(msFromDateAndTime(date, time)));
}

static
struct io_libecc_Value setSeconds (struct io_libecc_Context * const context)
{
	struct date date;
	struct time time;
	double s, ms;
	
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	msToDateAndTime(toLocal(context->this.data.date->ms), &date, &time);
	s = binaryArgumentOr(context, 0, NAN);
	ms = binaryArgumentOr(context, 1, time.ms);
	if (isnan(s) || isnan(ms))
		return io_libecc_Value.binary(context->this.data.date->ms = NAN);
	
	time.s = s;
	time.ms = ms;
	return io_libecc_Value.binary(context->this.data.date->ms = msClip(toUTC(msFromDateAndTime(date, time))));
}

static
struct io_libecc_Value setUTCSeconds (struct io_libecc_Context * const context)
{
	struct date date;
	struct time time;
	double s, ms;
	
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	msToDateAndTime(context->this.data.date->ms, &date, &time);
	s = binaryArgumentOr(context, 0, NAN);
	ms = binaryArgumentOr(context, 1, time.ms);
	if (isnan(s) || isnan(ms))
		return io_libecc_Value.binary(context->this.data.date->ms = NAN);
	
	time.s = s;
	time.ms = ms;
	return io_libecc_Value.binary(context->this.data.date->ms = msClip(msFromDateAndTime(date, time)));
}

static
struct io_libecc_Value setMinutes (struct io_libecc_Context * const context)
{
	struct date date;
	struct time time;
	double m, s, ms;
	
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	msToDateAndTime(toLocal(context->this.data.date->ms), &date, &time);
	m = binaryArgumentOr(context, 0, NAN);
	s = binaryArgumentOr(context, 1, time.s);
	ms = binaryArgumentOr(context, 2, time.ms);
	if (isnan(m) || isnan(s) || isnan(ms))
		return io_libecc_Value.binary(context->this.data.date->ms = NAN);
	
	time.m = m;
	time.s = s;
	time.ms = ms;
	return io_libecc_Value.binary(context->this.data.date->ms = msClip(toUTC(msFromDateAndTime(date, time))));
}

static
struct io_libecc_Value setUTCMinutes (struct io_libecc_Context * const context)
{
	struct date date;
	struct time time;
	double m, s, ms;
	
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	msToDateAndTime(context->this.data.date->ms, &date, &time);
	m = binaryArgumentOr(context, 0, NAN);
	s = binaryArgumentOr(context, 1, time.s);
	ms = binaryArgumentOr(context, 2, time.ms);
	if (isnan(m) || isnan(s) || isnan(ms))
		return io_libecc_Value.binary(context->this.data.date->ms = NAN);
	
	time.m = m;
	time.s = s;
	time.ms = ms;
	return io_libecc_Value.binary(context->this.data.date->ms = msClip(msFromDateAndTime(date, time)));
}

static
struct io_libecc_Value setHours (struct io_libecc_Context * const context)
{
	struct date date;
	struct time time;
	double h, m, s, ms;
	
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	msToDateAndTime(toLocal(context->this.data.date->ms), &date, &time);
	h = binaryArgumentOr(context, 0, NAN);
	m = binaryArgumentOr(context, 1, time.m);
	s = binaryArgumentOr(context, 2, time.s);
	ms = binaryArgumentOr(context, 3, time.ms);
	if (isnan(h) || isnan(m) || isnan(s) || isnan(ms))
		return io_libecc_Value.binary(context->this.data.date->ms = NAN);
	
	time.h = h;
	time.m = m;
	time.s = s;
	time.ms = ms;
	return io_libecc_Value.binary(context->this.data.date->ms = msClip(toUTC(msFromDateAndTime(date, time))));
}

static
struct io_libecc_Value setUTCHours (struct io_libecc_Context * const context)
{
	struct date date;
	struct time time;
	double h, m, s, ms;
	
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	msToDateAndTime(context->this.data.date->ms, &date, &time);
	h = binaryArgumentOr(context, 0, NAN);
	m = binaryArgumentOr(context, 1, time.m);
	s = binaryArgumentOr(context, 2, time.s);
	ms = binaryArgumentOr(context, 3, time.ms);
	if (isnan(h) || isnan(m) || isnan(s) || isnan(ms))
		return io_libecc_Value.binary(context->this.data.date->ms = NAN);
	
	time.h = h;
	time.m = m;
	time.s = s;
	time.ms = ms;
	return io_libecc_Value.binary(context->this.data.date->ms = msClip(msFromDateAndTime(date, time)));
}

static
struct io_libecc_Value setDate (struct io_libecc_Context * const context)
{
	struct date date;
	double day, ms;
	
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	ms = msToDate(toLocal(context->this.data.date->ms), &date);
	day = binaryArgumentOr(context, 0, NAN);
	if (isnan(day))
		return io_libecc_Value.binary(context->this.data.date->ms = NAN);
	
	date.day = day;
	return io_libecc_Value.binary(context->this.data.date->ms = msClip(toUTC(ms + msFromDate(date))));
}

static
struct io_libecc_Value setUTCDate (struct io_libecc_Context * const context)
{
	struct date date;
	double day, ms;
	
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	ms = msToDate(context->this.data.date->ms, &date);
	day = binaryArgumentOr(context, 0, NAN);
	if (isnan(day))
		return io_libecc_Value.binary(context->this.data.date->ms = NAN);
	
	date.day = day;
	return io_libecc_Value.binary(context->this.data.date->ms = msClip(ms + msFromDate(date)));
}

static
struct io_libecc_Value setMonth (struct io_libecc_Context * const context)
{
	struct date date;
	double month, day, ms;
	
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	ms = msToDate(toLocal(context->this.data.date->ms), &date);
	month = binaryArgumentOr(context, 0, NAN) + 1;
	day = binaryArgumentOr(context, 1, date.day);
	if (isnan(month) || isnan(day))
		return io_libecc_Value.binary(context->this.data.date->ms = NAN);
	
	date.month = month;
	date.day = day;
	return io_libecc_Value.binary(context->this.data.date->ms = msClip(toUTC(ms + msFromDate(date))));
}

static
struct io_libecc_Value setUTCMonth (struct io_libecc_Context * const context)
{
	struct date date;
	double month, day, ms;
	
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	ms = msToDate(context->this.data.date->ms, &date);
	month = binaryArgumentOr(context, 0, NAN) + 1;
	day = binaryArgumentOr(context, 1, date.day);
	if (isnan(month) || isnan(day))
		return io_libecc_Value.binary(context->this.data.date->ms = NAN);
	
	date.month = month;
	date.day = day;
	return io_libecc_Value.binary(context->this.data.date->ms = msClip(ms + msFromDate(date)));
}

static
struct io_libecc_Value setFullYear (struct io_libecc_Context * const context)
{
	struct date date;
	double year, month, day, ms;
	
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	if (isnan(context->this.data.date->ms))
		context->this.data.date->ms = 0;
	
	ms = msToDate(toLocal(context->this.data.date->ms), &date);
	year = binaryArgumentOr(context, 0, NAN);
	month = binaryArgumentOr(context, 1, date.month - 1) + 1;
	day = binaryArgumentOr(context, 2, date.day);
	if (isnan(year) || isnan(month) || isnan(day))
		return io_libecc_Value.binary(context->this.data.date->ms = NAN);
	
	date.year = year;
	date.month = month;
	date.day = day;
	return io_libecc_Value.binary(context->this.data.date->ms = msClip(toUTC(ms + msFromDate(date))));
}

static
struct io_libecc_Value setYear (struct io_libecc_Context * const context)
{
	struct date date;
	double year, month, day, ms;
	
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	if (isnan(context->this.data.date->ms))
		context->this.data.date->ms = 0;
	
	ms = msToDate(toLocal(context->this.data.date->ms), &date);
	year = binaryArgumentOr(context, 0, NAN);
	month = binaryArgumentOr(context, 1, date.month - 1) + 1;
	day = binaryArgumentOr(context, 2, date.day);
	if (isnan(year) || isnan(month) || isnan(day))
		return io_libecc_Value.binary(context->this.data.date->ms = NAN);
	
	date.year = year < 100? year + 1900: year;
	date.month = month;
	date.day = day;
	return io_libecc_Value.binary(context->this.data.date->ms = msClip(toUTC(ms + msFromDate(date))));
}

static
struct io_libecc_Value setUTCFullYear (struct io_libecc_Context * const context)
{
	struct date date;
	double year, month, day, ms;
	
	io_libecc_Context.assertThisType(context, io_libecc_value_dateType);
	
	if (isnan(context->this.data.date->ms))
		context->this.data.date->ms = 0;
	
	ms = msToDate(context->this.data.date->ms, &date);
	year = binaryArgumentOr(context, 0, NAN);
	month = binaryArgumentOr(context, 1, date.month - 1) + 1;
	day = binaryArgumentOr(context, 2, date.day);
	if (isnan(year) || isnan(month) || isnan(day))
		return io_libecc_Value.binary(context->this.data.date->ms = NAN);
	
	date.year = year;
	date.month = month;
	date.day = day;
	return io_libecc_Value.binary(context->this.data.date->ms = msClip(ms + msFromDate(date)));
}

static
struct io_libecc_Value now (struct io_libecc_Context * const context)
{
	return io_libecc_Value.binary(msClip(io_libecc_Env.currentTime()));
}

static
struct io_libecc_Value parse (struct io_libecc_Context * const context)
{
	struct io_libecc_Value value;
	
	value = io_libecc_Value.toString(context, io_libecc_Context.argument(context, 0));
	
	return io_libecc_Value.binary(msClip(msFromBytes(io_libecc_Value.stringBytes(&value), io_libecc_Value.stringLength(&value))));
}

static
struct io_libecc_Value UTC (struct io_libecc_Context * const context)
{
	if (io_libecc_Context.argumentCount(context) > 1)
		return io_libecc_Value.binary(msClip(msFromArguments(context)));
	
	return io_libecc_Value.binary(NAN);
}

static
struct io_libecc_Value constructor (struct io_libecc_Context * const context)
{
	double time;
	uint16_t count;
	
	if (!context->construct)
		return io_libecc_Value.chars(msToChars(io_libecc_Env.currentTime(), localOffset));
	
	count = io_libecc_Context.argumentCount(context);
	
	if (count > 1)
		time = toUTC(msFromArguments(context));
	else if (count)
	{
		struct io_libecc_Value value = io_libecc_Value.toPrimitive(context, io_libecc_Context.argument(context, 0), io_libecc_value_hintAuto);
		
		if (io_libecc_Value.isString(value))
			time = msFromBytes(io_libecc_Value.stringBytes(&value), io_libecc_Value.stringLength(&value));
		else
			time = io_libecc_Value.toBinary(context, value).data.binary;
	}
	else
		time = io_libecc_Env.currentTime();
	
	return io_libecc_Value.date(create(time));
}

// MARK: - Methods

void setup (void)
{
	const enum io_libecc_value_Flags h = io_libecc_value_hidden;
	
	setupLocalOffset();
	
	io_libecc_Function.setupBuiltinObject(
		&io_libecc_date_constructor, constructor, -7,
		&io_libecc_date_prototype, io_libecc_Value.date(create(NAN)),
		&io_libecc_date_type);
	
	io_libecc_Function.addMethod(io_libecc_date_constructor, "parse", parse, 1, h);
	io_libecc_Function.addMethod(io_libecc_date_constructor, "UTC", UTC, -7, h);
	io_libecc_Function.addMethod(io_libecc_date_constructor, "now", now, 0, h);
	
	io_libecc_Function.addToObject(io_libecc_date_prototype, "toString", toString, 0, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "toDateString", toDateString, 0, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "toTimeString", toTimeString, 0, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "toLocaleString", toString, 0, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "toLocaleDateString", toDateString, 0, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "toLocaleTimeString", toTimeString, 0, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "valueOf", valueOf, 0, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "getTime", valueOf, 0, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "getYear", getYear, 0, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "getFullYear", getFullYear, 0, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "getUTCFullYear", getUTCFullYear, 0, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "getMonth", getMonth, 0, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "getUTCMonth", getUTCMonth, 0, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "getDate", getDate, 0, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "getUTCDate", getUTCDate, 0, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "getDay", getDay, 0, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "getUTCDay", getUTCDay, 0, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "getHours", getHours, 0, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "getUTCHours", getUTCHours, 0, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "getMinutes", getMinutes, 0, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "getUTCMinutes", getUTCMinutes, 0, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "getSeconds", getSeconds, 0, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "getUTCSeconds", getUTCSeconds, 0, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "getMilliseconds", getMilliseconds, 0, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "getUTCMilliseconds", getUTCMilliseconds, 0, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "getTimezoneOffset", getTimezoneOffset, 0, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "setTime", setTime, 1, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "setMilliseconds", setMilliseconds, 1, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "setUTCMilliseconds", setUTCMilliseconds, 1, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "setSeconds", setSeconds, 2, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "setUTCSeconds", setUTCSeconds, 2, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "setMinutes", setMinutes, 3, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "setUTCMinutes", setUTCMinutes, 3, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "setHours", setHours, 4, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "setUTCHours", setUTCHours, 4, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "setDate", setDate, 1, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "setUTCDate", setUTCDate, 1, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "setMonth", setMonth, 2, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "setUTCMonth", setUTCMonth, 2, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "setYear", setYear, 3, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "setFullYear", setFullYear, 3, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "setUTCFullYear", setUTCFullYear, 3, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "toUTCString", toUTCString, 0, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "toGMTString", toUTCString, 0, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "toISOString", toISOString, 0, h);
	io_libecc_Function.addToObject(io_libecc_date_prototype, "toJSON", toJSON, 1, h);
}

void teardown (void)
{
	io_libecc_date_prototype = NULL;
	io_libecc_date_constructor = NULL;
}

struct io_libecc_Date *create (double ms)
{
	struct io_libecc_Date *self = malloc(sizeof(*self));
	*self = io_libecc_Date.identity;
	io_libecc_Pool.addObject(&self->object);
	io_libecc_Object.initialize(&self->object, io_libecc_date_prototype);
	
	self->ms = msClip(ms);
	
	return self;
}
