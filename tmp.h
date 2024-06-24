
	#define io_libecc_key_Keys \
		_( prototype )\
		_( constructor )\
		_( length )\
		_( arguments )\
		_( callee )\
		_( name )\
		_( message )\
		_( toString )\
		_( valueOf )\
		_( eval )\
		_( value )\
		_( writable )\
		_( enumerable )\
		_( configurable )\
		_( get )\
		_( set )\
		_( join )\
		_( toISOString )\
		_( input )\
		_( index )\
		_( lastIndex )\
		_( global )\
		_( ignoreCase )\
		_( multiline )\
		_( source )\
		\



		#define _(X) \
            { \
                cstr = #X; \
                io_libecc_key_##X = addWithText(io_libecc_Text.make(cstr, strlen(cstr)), 0); \
            }
		io_libecc_key_Keys
		#undef _
