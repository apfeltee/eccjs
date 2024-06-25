"use strict";

/*
 * via:
 * https://github.com/chidiwilliams/jscheme
 * https://chidiwilliams.com/post/how-to-write-a-lisp-interpreter-in-javascript/
 *
 * note that this has been mangled through babel, since mujs only supports ~es5(-ish)
 */

function _createForOfIteratorHelper(o, allowArrayLike) {
    var it = typeof Symbol != "undefined" && o[Symbol.iterator] || o["@@iterator"];
    if (!it) {
        if (Array.isArray(o) || (it = _unsupportedIterableToArray(o)) || allowArrayLike && o && typeof o.length == "number") {
            if (it) o = it;
            var i = 0;
            var F = function F() {};
            return {
                s: F,
                n: function n() {
                    if (i >= o.length) return {
                        done: true
                    };
                    return {
                        done: false,
                        value: o[i++]
                    };
                },
                e: function e(_e2) {
                    throw _e2;
                },
                f: F
            };
        }
        throw new TypeError("Invalid attempt to iterate non-iterable instance.\nIn order to be iterable, non-array objects must have a [Symbol.iterator]() method.");
    }
    var normalCompletion = true,
        didErr = false,
        err;
    return {
        s: function s() {
            it = it.call(o);
        },
        n: function n() {
            var step = it.next();
            normalCompletion = step.done;
            return step;
        },
        e: function e(_e3) {
            didErr = true;
            err = _e3;
        },
        f: function f() {
            try {
                if (!normalCompletion && it.return != null) it.return();
            } finally {
                if (didErr) throw err;
            }
        }
    };
}

function _toConsumableArray(arr) {
    return _arrayWithoutHoles(arr) || _iterableToArray(arr) || _unsupportedIterableToArray(arr) || _nonIterableSpread();
}

function _nonIterableSpread() {
    throw new TypeError("Invalid attempt to spread non-iterable instance.\nIn order to be iterable, non-array objects must have a [Symbol.iterator]() method.");
}

function _iterableToArray(iter) {
    if (typeof Symbol != "undefined" && iter[Symbol.iterator] != null || iter["@@iterator"] != null) return Array.from(iter);
}

function _arrayWithoutHoles(arr) {
    if (Array.isArray(arr)) return _arrayLikeToArray(arr);
}

function _slicedToArray(arr, i) {
    return _arrayWithHoles(arr) || _iterableToArrayLimit(arr, i) || _unsupportedIterableToArray(arr, i) || _nonIterableRest();
}

function _nonIterableRest() {
    throw new TypeError("Invalid attempt to destructure non-iterable instance.\nIn order to be iterable, non-array objects must have a [Symbol.iterator]() method.");
}

function _unsupportedIterableToArray(o, minLen) {
    if (!o) return;
    if (typeof o == "string") return _arrayLikeToArray(o, minLen);
    var n = Object.prototype.toString.call(o).slice(8, -1);
    if (n == "Object" && o.constructor) n = o.constructor.name;
    if (n == "Map" || n == "Set") return Array.from(o);
    if (n == "Arguments" || /^(?:Ui|I)nt(?:8|16|32)(?:Clamped)?Array$/.test(n)) return _arrayLikeToArray(o, minLen);
}

function _arrayLikeToArray(arr, len) {
    if (len == null || len > arr.length) len = arr.length;
    for (var i = 0, arr2 = new Array(len); i < len; i++) {
        arr2[i] = arr[i];
    }
    return arr2;
}

function _iterableToArrayLimit(arr, i) {
    var _i = arr == null ? null : typeof Symbol != "undefined" && arr[Symbol.iterator] || arr["@@iterator"];
    if (_i == null) return;
    var _arr = [];
    var _n = true;
    var _d = false;
    var _s, _e;
    try {
        for (_i = _i.call(arr); !(_n = (_s = _i.next()).done); _n = true) {
            _arr.push(_s.value);
            if (i && _arr.length == i) break;
        }
    } catch (err) {
        _d = true;
        _e = err;
    } finally {
        try {
            if (!_n && _i["return"] != null) _i["return"]();
        } finally {
            if (_d) throw _e;
        }
    }
    return _arr;
}

function _arrayWithHoles(arr) {
    if (Array.isArray(arr)) return arr;
}

function _inherits(subClass, superClass) {
    if (typeof superClass != "function" && superClass != null) {
        throw new TypeError("Super expression must either be null or a function");
    }
    subClass.prototype = Object.create(superClass && superClass.prototype, {
        constructor: {
            value: subClass,
            writable: true,
            configurable: true
        }
    });
    Object.defineProperty(subClass, "prototype", {
        writable: false
    });
    if (superClass) _setPrototypeOf(subClass, superClass);
}

function _createSuper(Derived) {
    var hasNativeReflectConstruct = _isNativeReflectConstruct();
    return function _createSuperInternal() {
        var Super = _getPrototypeOf(Derived),
            result;
        if (hasNativeReflectConstruct) {
            var NewTarget = _getPrototypeOf(this).constructor;
            result = Reflect.construct(Super, arguments, NewTarget);
        } else {
            result = Super.apply(this, arguments);
        }
        return _possibleConstructorReturn(this, result);
    };
}

function _possibleConstructorReturn(self, call) {
    if (call && (typeof call == "object" || typeof call == "function")) {
        return call;
    } else if (call != void 0) {
        throw new TypeError("Derived constructors may only return object or undefined");
    }
    return _assertThisInitialized(self);
}

function _assertThisInitialized(self) {
    if (self == void 0) {
        throw new ReferenceError("this hasn't been initialised - super() hasn't been called");
    }
    return self;
}

function _wrapNativeSuper(Class) {
    var _cache = typeof Map == "function" ? new Map() : undefined;
    _wrapNativeSuper = function _wrapNativeSuper(Class) {
        if (Class == null || !_isNativeFunction(Class)) return Class;
        if (typeof Class != "function") {
            throw new TypeError("Super expression must either be null or a function");
        }
        if (typeof _cache != "undefined") {
            if (_cache.has(Class)) return _cache.get(Class);
            _cache.set(Class, Wrapper);
        }

        function Wrapper() {
            return _construct(Class, arguments, _getPrototypeOf(this).constructor);
        }
        Wrapper.prototype = Object.create(Class.prototype, {
            constructor: {
                value: Wrapper,
                enumerable: false,
                writable: true,
                configurable: true
            }
        });
        return _setPrototypeOf(Wrapper, Class);
    };
    return _wrapNativeSuper(Class);
}

function _construct(Parent, args, Class) {
    if (_isNativeReflectConstruct()) {
        _construct = Reflect.construct.bind();
    } else {
        _construct = function _construct(Parent, args, Class) {
            var a = [null];
            a.push.apply(a, args);
            var Constructor = Function.bind.apply(Parent, a);
            var instance = new Constructor();
            if (Class) _setPrototypeOf(instance, Class.prototype);
            return instance;
        };
    }
    return _construct.apply(null, arguments);
}

function _isNativeReflectConstruct() {
    if (typeof Reflect == "undefined" || !Reflect.construct) return false;
    if (Reflect.construct.sham) return false;
    if (typeof Proxy == "function") return true;
    try {
        Boolean.prototype.valueOf.call(Reflect.construct(Boolean, [], function() {}));
        return true;
    } catch (e) {
        return false;
    }
}

function _isNativeFunction(fn) {
    return Function.toString.call(fn).indexOf("[native code]") != -1;
}

function _setPrototypeOf(o, p) {
    _setPrototypeOf = Object.setPrototypeOf ? Object.setPrototypeOf.bind() : function _setPrototypeOf(o, p) {
        o.__proto__ = p;
        return o;
    };
    return _setPrototypeOf(o, p);
}

function _getPrototypeOf(o) {
    _getPrototypeOf = Object.setPrototypeOf ? Object.getPrototypeOf.bind() : function _getPrototypeOf(o) {
        return o.__proto__ || Object.getPrototypeOf(o);
    };
    return _getPrototypeOf(o);
}

function _defineProperties(target, props) {
    for (var i = 0; i < props.length; i++) {
        var descriptor = props[i];
        descriptor.enumerable = descriptor.enumerable || false;
        descriptor.configurable = true;
        if ("value" in descriptor) descriptor.writable = true;
        Object.defineProperty(target, descriptor.key, descriptor);
    }
}

function _createClass(Constructor, protoProps, staticProps) {
    if (protoProps) _defineProperties(Constructor.prototype, protoProps);
    if (staticProps) _defineProperties(Constructor, staticProps);
    Object.defineProperty(Constructor, "prototype", {
        writable: false
    });
    return Constructor;
}

function _defineProperty(obj, key, value) {
    if (key in obj) {
        Object.defineProperty(obj, key, {
            value: value,
            enumerable: true,
            configurable: true,
            writable: true
        });
    } else {
        obj[key] = value;
    }
    return obj;
}
/*
 * via:
 * https://github.com/chidiwilliams/jscheme
 * https://chidiwilliams.com/post/how-to-write-a-lisp-interpreter-in-javascript/
 */
/**
 * The Scanner scans a source string into a list of tokens
 */
var Scanner = /*#__PURE__*/ function() {
    /**
     * @type {Token[]}
     */

    /**
     *
     * @param {string} source
     */
    function Scanner(source) {
        _defineProperty(this, "start", 0);
        _defineProperty(this, "current", 0);
        _defineProperty(this, "line", 0);
        _defineProperty(this, "tokens", []);
        this.source = source;
    }

    /**
     * Scans the source string and returns all the found tokens
     */
    _createClass(Scanner, [{
        key: "scan",
        value: function scan() {
            try {
                while (!this.isAtEnd()) {
                    this.start = this.current;
                    var char = this.advance();
                    switch (char) {
                        case '(':
                            this.addToken(TokenType.LeftBracket);
                            break;
                        case ')':
                            this.addToken(TokenType.RightBracket);
                            break;
                        case ' ':
                        case '\r':
                        case '\t':
                            break;
                        case '\n':
                            this.line++;
                            break;
                        case '#':
                            if (this.peek() == 't') {
                                this.advance();
                                this.addToken(TokenType.Boolean, true);
                                break;
                            }
                            if (this.peek() == 'f') {
                                this.advance();
                                this.addToken(TokenType.Boolean, false);
                                break;
                            }
                            case '"':
                                while (this.peek() != '"' && !this.isAtEnd()) {
                                    this.advance();
                                }
                                var literal = this.source.slice(this.start + 1, this.current);
                                this.addToken(TokenType.String, literal);
                                this.advance();
                                break;
                            case ';':
                                // Comment - ignore all characters until the end of the line
                                while (this.peek() != '\n' && !this.isAtEnd()) {
                                    this.advance();
                                }
                                break;
                            case '`':
                                this.addToken(TokenType.Quasiquote);
                                break;
                            case "'":
                                this.addToken(TokenType.Quote);
                                break;
                            case ',':
                                if (this.peek() == '@') {
                                    this.advance();
                                    this.addToken(TokenType.UnquoteSplicing);
                                } else {
                                    this.addToken(TokenType.Unquote);
                                }
                                break;
                            default:
                                if (this.isDigit(char)) {
                                    while (this.isDigitOrDot(this.peek())) {
                                        this.advance();
                                    }
                                    var numberAsString = this.source.slice(this.start, this.current);
                                    var _literal = parseFloat(numberAsString);
                                    this.addToken(TokenType.Number, _literal);
                                    break;
                                }
                                if (this.isIdentifier(char)) {
                                    while (this.isIdentifier(this.peek())) {
                                        this.advance();
                                    }
                                    this.addToken(TokenType.Symbol);
                                    break;
                                }
                                throw new SyntaxError(this.line, 'Unknown token ' + char);
                    }
                }
                this.tokens.push(new Token(TokenType.Eof, 0, '', null, this.line));
                return {
                    tokens: this.tokens,
                    hadError: false
                };
            } catch (error) {
                if (error instanceof SyntaxError) {
                    console.error(error.toString());
                    return {
                        tokens: this.tokens,
                        hadError: true
                    };
                }
                throw error;
            }
        }

        /**
         * Returns true if char is is valid symbol character
         *
         * @param {string} char
         */
    }, {
        key: "isIdentifier",
        value: function isIdentifier(ch) {
            var i;
            var things = ['+', '-', '.', '*', '/', '<', '=', '>', '!', '?', ':', '$', '%', '_', '&', '~', '^'];
            if ((this.isDigitOrDot(ch) || ch >= 'A' && ch <= 'Z' || ch >= 'a' && ch <= 'z')) {
                return true;
            }
            for (i = 0; i < things.length; i++) {
                if (ch == things[i]) {
                    return true;
                }
            }
            return false;
        }

        /**
         * Returns the current character and moves the scanner forward by one
         */
    }, {
        key: "advance",
        value: function advance() {
            return this.source[this.current++];
        }

        /**
         * Returns the current character
         */
    }, {
        key: "peek",
        value: function peek() {
            return this.source[this.current];
        }

        /**
         * Returns true if char is a digit
         * @param {string} char
         */
    }, {
        key: "isDigit",
        value: function isDigit(char) {
            return char >= '0' && char <= '9';
        }

        /**
         * Returns true if char is a digit or a dot
         * @param {string} char
         */
    }, {
        key: "isDigitOrDot",
        value: function isDigitOrDot(char) {
            return this.isDigit(char) || char == '.';
        }

        /**
         * Adds a token with the given type and literal value to the token list
         * @param {TokenType} tokenType
         * @param {any} literal
         */
    }, {
        key: "addToken",
        value: function addToken(tokenType, literal) {
            var lexeme = this.source.slice(this.start, this.current);
            var token = new Token(tokenType, this.start, lexeme, literal, this.line);
            this.tokens.push(token);
        }

        /**
         * Returns true if scanning is complete
         */
    }, {
        key: "isAtEnd",
        value: function isAtEnd() {
            return this.current >= this.source.length;
        }
    }]);
    return Scanner;
}();
/**
 * @enum {string}
 */
var TokenType = {
    LeftBracket: 'LeftBracket',
    RightBracket: 'RightBracket',
    Symbol: 'Symbol',
    Number: 'Number',
    Boolean: 'Boolean',
    String: 'String',
    Quasiquote: 'Quasiquote',
    Quote: 'Quote',
    Unquote: 'Unquote',
    UnquoteSplicing: 'UnquoteSplicing',
    Eof: 'Eof'
};

/**
 * A Token is a useful unit of the program source
 */
var Token = /*#__PURE__*/ _createClass(
    /**
     * @param {TokenType} tokenType
     * @param {number} start
     * @param {string} lexeme
     * @param {any} literal
     * @param {number} line
     */
    function Token(tokenType, start, lexeme, literal, line) {
        this.tokenType = tokenType;
        this.start = start;
        this.lexeme = lexeme;
        this.literal = literal;
        this.line = line;
    });
/**
 * A "compile-time" error that occurs during scanning or parsing.
 */
var SyntaxError = /*#__PURE__*/ function(_Error) {
    _inherits(SyntaxError, _Error);
    var _super = _createSuper(SyntaxError);
    /**
     *
     * @param {string} message
     * @param {number} line
     */
    function SyntaxError(line, message) {
        var _this;
        _this = _super.call(this, message);
        _this.line = line;
        return _this;
    }
    _createClass(SyntaxError, [{
        key: "toString",
        value: function toString() {
            return "SyntaxError: [line: " + this.line + "] " + this.message + "";
        }
    }]);
    return SyntaxError;
}( /*#__PURE__*/ _wrapNativeSuper(Error));
/**
 * Value of null and the empty list
 */
var NULL_VALUE = [];

/**
 * The Parser parses a list of tokens into an AST
 *
 * Parser grammar:
 * program     => expression*
 * expression  => lambda | define | if | set! | let | quote
 *               | "(" expression* ")" | () | atom
 * define      => "(" "define" SYMBOL expression ")"
 * if          => "(" "if" expression expression expression? ")"
 * set!        => "(" "set" SYMBOL expression ")"
 * let         => "(" "let" "(" let-binding* ")" expression* ")"
 * let-binding => "(" SYMBOL expression ")"
 * atom        => SYMBOL | NUMBER | TRUE | FALSE | STRING
 */
var Parser = /*#__PURE__*/ function() {
    /*
     * @param {Token[]} tokens
     */
    function Parser(tokens) {
        _defineProperty(this, "current", 0);
        this.tokens = tokens;
    }

    /**
     * Parses the expressions in the token list
     */
    _createClass(Parser, [{
        key: "parse",
        value: function parse() {
            try {
                var expressions = [];
                while (!this.isAtEnd()) {
                    var expr = this.expression();
                    expressions.push(expr);
                }
                return expressions;
            } catch (error) {
                if (error instanceof SyntaxError) {
                    console.error(error.toString());
                    return [];
                }
                throw error;
            }
        }

        /**
         * Parses a list expression
         * @returns {Expr}
         */
    }, {
        key: "expression",
        value: function expression() {
            if (this.match(TokenType.LeftBracket)) {
                if (this.match(TokenType.RightBracket)) {
                    return new LiteralExpr(NULL_VALUE);
                }
                var token = this.peek();
                if (token.lexeme == 'lambda') {
                    return this["lambda"]();
                }
                if (token.lexeme == 'define') {
                    return this["define"]();
                }
                if (token.lexeme == 'if') {
                    return this["if"]();
                }
                if (token.lexeme == 'set!') {
                    return this["set"]();
                }
                if (token.lexeme == 'let') {
                    return this["let"]();
                }
                if (token.lexeme == 'quote') {
                    return this["quote"]();
                }
                return this.call();
            }
            return this.atom();
        }

        /**
         * Parses a call expression
         * @returns {Expr}
         */
    }, {
        key: "call",
        value: function call() {
            var callee = this.expression();
            var args = [];
            while (!this.match(TokenType.RightBracket)) {
                args.push(this.expression());
            }
            return new CallExpr(callee, args);
        }

        /**
         * Parses a lambda expression
         * @returns {Expr}
         */
    }, {
        key: "lambda",
        value: function lambda() {
            this.advance();

            /**
             * @type {Token[] | Token}
             */
            var params;
            if (this.match(TokenType.Symbol)) {
                params = this.previous();
            } else {
                params = [];
                this.consume(TokenType.LeftBracket);
                while (!this.match(TokenType.RightBracket)) {
                    var token = this.consume(TokenType.Symbol);
                    params.push(token);
                }
            }

            /**
             * @type {Expr[]}
             */
            var body = [];
            while (!this.match(TokenType.RightBracket)) {
                body.push(this.expression());
            }
            return new LambdaExpr(params, body);
        }

        /**
         * Parses a define expression
         * @returns {Expr}
         */
    }, {
        key: "define",
        value: function define() {
            this.advance();
            var name = this.consume(TokenType.Symbol);
            var value = this.expression();
            this.consume(TokenType.RightBracket);
            return new DefineExpr(name, value);
        }

        /**
         * Parses an if expression
         * @returns {Expr}
         */
    }, {
        key: "if",
        value: function _if() {
            this.advance();
            var condition = this.expression();
            var thenBranch = this.expression();
            var elseBranch;
            if (!this.match(TokenType.RightBracket)) {
                elseBranch = this.expression();
            }
            this.consume(TokenType.RightBracket);
            return new IfExpr(condition, thenBranch, elseBranch);
        }

        /**
         * Parses a set expression
         * @returns {Expr}
         */
    }, {
        key: "set",
        value: function set() {
            this.advance();
            var name = this.consume(TokenType.Symbol);
            var value = this.expression();
            this.consume(TokenType.RightBracket);
            return new SetExpr(name, value);
        }

        /**
         * Parses a let expression
         * @returns {Expr}
         */
    }, {
        key: "let",
        value: function _let() {
            this.advance();
            this.consume(TokenType.LeftBracket);
            var bindings = [];
            while (!this.match(TokenType.RightBracket)) {
                bindings.push(this.letBinding());
            }

            /**
             * @type {Expr[]}
             */
            var body = [];
            while (!this.match(TokenType.RightBracket)) {
                body.push(this.expression());
            }
            return new LetExpr(bindings, body);
        }
    }, {
        key: "letBinding",
        value: function letBinding() {
            this.consume(TokenType.LeftBracket);
            var name = this.consume(TokenType.Symbol);
            var value = this.expression();
            this.consume(TokenType.RightBracket);
            return new LetBindingNode(name, value);
        }
    }, {
        key: "quote",
        value: function quote() {
            this.advance();
            var value = this.quoteValue();
            this.consume(TokenType.RightBracket);
            return new QuoteExpr(value);
        }
    }, {
        key: "quoteValue",
        value: function quoteValue() {
            var quoted;
            if (this.match(TokenType.LeftBracket)) {
                var args = [];
                while (!this.match(TokenType.RightBracket)) {
                    var params = this.expression();
                    args.push(params);
                }
                quoted = new ListExpr(args);
            } else {
                quoted = this.expression();
            }
            return quoted;
        }

        /**
         * Parses an atom expression
         * @returns {Expr}
         */
    }, {
        key: "atom",
        value: function atom() {
            switch (true) {
                case this.match(TokenType.Symbol):
                    return new SymbolExpr(this.previous());
                case this.match(TokenType.Number):
                case this.match(TokenType.String):
                case this.match(TokenType.Boolean):
                    return new LiteralExpr(this.previous().literal);
                case this.match(TokenType.Quote):
                    return this.quoteValue();
                default:
                    throw new SyntaxError(this.peek().line, "Unexpected token: " + this.peek().tokenType + "");
            }
        }

        /**
         * Returns true if all tokens have been parsed
         */
    }, {
        key: "isAtEnd",
        value: function isAtEnd() {
            return this.peek().tokenType == TokenType.Eof;
        }

        /**
         * Returns the current token if it matches the given token type. Else, it throws an error.
         * @param {TokenType} tokenType
         */
    }, {
        key: "consume",
        value: function consume(tokenType) {
            if (this.check(tokenType)) {
                return this.advance();
            }
            throw new SyntaxError(this.previous().line, "Unexpected token " + this.previous().tokenType + ", expected " + tokenType + "");
        }

        /**
         * Returns the current token and advances to the next token
         */
    }, {
        key: "advance",
        value: function advance() {
            if (!this.isAtEnd()) {
                this.current++;
            }
            return this.previous();
        }

        /**
         * Returns true if the current token matches the given token type and then advances
         * @param {TokenType} tokenType
         */
    }, {
        key: "match",
        value: function match(tokenType) {
            if (this.check(tokenType)) {
                this.current++;
                return true;
            }
            return false;
        }

        /**
         * Returns true if the current token matches the given token type
         * @param {TokenType} tokenType
         */
    }, {
        key: "check",
        value: function check(tokenType) {
            return this.peek().tokenType == tokenType;
        }

        /**
         * Returns the current token
         */
    }, {
        key: "peek",
        value: function peek() {
            return this.tokens[this.current];
        }

        /**
         * Returns the previous token
         */
    }, {
        key: "previous",
        value: function previous() {
            return this.tokens[this.current - 1];
        }
    }]);
    return Parser;
}();
var Expr = /*#__PURE__*/ _createClass(function Expr() {});
var CallExpr = /*#__PURE__*/ function(_Expr) {
    _inherits(CallExpr, _Expr);
    var _super2 = _createSuper(CallExpr);
    /**
     * @param {Expr} callee
     * @param {Expr[]} args
     */
    function CallExpr(callee, args) {
        var _this2;
        _this2 = _super2.call(this);
        _this2.callee = callee;
        _this2.args = args;
        return _this2;
    }
    return _createClass(CallExpr);
}(Expr);
var SymbolExpr = /*#__PURE__*/ function(_Expr2) {
    _inherits(SymbolExpr, _Expr2);
    var _super3 = _createSuper(SymbolExpr);
    /**
     * @param {Token} token
     */
    function SymbolExpr(token) {
        var _this3;
        _this3 = _super3.call(this);
        _this3.token = token;
        return _this3;
    }
    return _createClass(SymbolExpr);
}(Expr);
var LiteralExpr = /*#__PURE__*/ function(_Expr3) {
    _inherits(LiteralExpr, _Expr3);
    var _super4 = _createSuper(LiteralExpr);
    /**
     * @param {any} value
     */
    function LiteralExpr(value) {
        var _this4;
        _this4 = _super4.call(this);
        _this4.value = value;
        return _this4;
    }
    return _createClass(LiteralExpr);
}(Expr);
var LambdaExpr = /*#__PURE__*/ function(_Expr4) {
    _inherits(LambdaExpr, _Expr4);
    var _super5 = _createSuper(LambdaExpr);
    /**
     * @param {Token[] | Token} params
     * @param {Expr[]} body
     */
    function LambdaExpr(params, body) {
        var _this5;
        _this5 = _super5.call(this);
        _this5.params = params;
        _this5.body = body;
        return _this5;
    }
    return _createClass(LambdaExpr);
}(Expr);
var SetExpr = /*#__PURE__*/ function(_Expr5) {
    _inherits(SetExpr, _Expr5);
    var _super6 = _createSuper(SetExpr);
    /**
     * @param {Token} name
     * @param {Expr} value
     */
    function SetExpr(name, value) {
        var _this6;
        _this6 = _super6.call(this);
        _this6.name = name;
        _this6.value = value;
        return _this6;
    }
    return _createClass(SetExpr);
}(Expr);
var DefineExpr = /*#__PURE__*/ function(_Expr6) {
    _inherits(DefineExpr, _Expr6);
    var _super7 = _createSuper(DefineExpr);
    /**
     * @param {Token} name
     * @param {Expr} value
     */
    function DefineExpr(name, value) {
        var _this7;
        _this7 = _super7.call(this);
        _this7.name = name;
        _this7.value = value;
        return _this7;
    }
    return _createClass(DefineExpr);
}(Expr);
var IfExpr = /*#__PURE__*/ function(_Expr7) {
    _inherits(IfExpr, _Expr7);
    var _super8 = _createSuper(IfExpr);
    /**
     * @param {Expr} condition
     * @param {Expr} thenBranch
     * @param {Expr} elseBranch
     */
    function IfExpr(condition, thenBranch, elseBranch) {
        var _this8;
        _this8 = _super8.call(this);
        _this8.condition = condition;
        _this8.thenBranch = thenBranch;
        _this8.elseBranch = elseBranch;
        return _this8;
    }
    return _createClass(IfExpr);
}(Expr);
var LetExpr = /*#__PURE__*/ _createClass(
    /**
     * @param {LetBindingNode[]} bindings
     * @param {Expr[]} body
     */
    function LetExpr(bindings, body) {
        this.bindings = bindings;
        this.body = body;
    });
var LetBindingNode = /*#__PURE__*/ _createClass(
    /**
     * @param {Token} name
     * @param {Expr} value
     */
    function LetBindingNode(name, value) {
        this.name = name;
        this.value = value;
    });
var QuoteExpr = /*#__PURE__*/ _createClass(
    /**
     *
     * @param {Expr} value
     */
    function QuoteExpr(value) {
        this.value = value;
    });
var ListExpr = /*#__PURE__*/ function(_Expr8) {
    _inherits(ListExpr, _Expr8);
    var _super9 = _createSuper(ListExpr);
    /**
     *
     * @param {Expr[]} items
     */
    function ListExpr(items) {
        var _this9;
        _this9 = _super9.call(this);
        _this9.items = items;
        return _this9;
    }
    return _createClass(ListExpr);
}(Expr);
/**
 * The Interpreter interprets (or executes) the parsed expressions
 */
var Interpreter = /*#__PURE__*/ function() {
    function Interpreter() {
        var _this10 = this;
        // Setup the interpreter's environment with the primitive procedures
        var env = new Environment();
        env.define('*', new PrimitiveProcedure(function(args) {
            return args.reduce(function(a, b) {
                return a * b;
            }, 1);
        }));
        env.define('+', new PrimitiveProcedure(function(args) {
            return args.reduce(function(a, b) {
                return a + b;
            }, 0);
        }));
        env.define('-', new PrimitiveProcedure(function(args) {
            return args.reduce(function(a, b) {
                return a - b;
            });
        }));
        env.define('/', new PrimitiveProcedure(function(args) {
            return args.reduce(function(a, b) {
                return a / b;
            });
        }));
        env.define('=', new PrimitiveProcedure(function(args) {
            return args.reduce(function(a, b) {
                return a == b;
            });
        }));
        env.define('<=', new PrimitiveProcedure(function(args) {
            return args.reduce(function(a, b) {
                return a <= b;
            });
        }));
        env.define('>=', new PrimitiveProcedure(function(args) {
            return args.reduce(function(a, b) {
                return a >= b;
            });
        }));
        env.define('string-length', new PrimitiveProcedure(function(_ref) {
            var _ref2 = _slicedToArray(_ref, 1),
                str = _ref2[0];
            return str.length;
        }));
        env.define('string-append', new PrimitiveProcedure(function(args) {
            return args.reduce(function(a, b) {
                return a + b;
            });
        }));
        env.define('list', new PrimitiveProcedure(function(args) {
            return args;
        }));
        env.define('null?', new PrimitiveProcedure(function(_ref3) {
            var _ref4 = _slicedToArray(_ref3, 1),
                arg = _ref4[0];
            return arg == NULL_VALUE;
        }));
        env.define('list?', new PrimitiveProcedure(function(_ref5) {
            var _ref6 = _slicedToArray(_ref5, 1),
                arg = _ref6[0];
            return arg instanceof Array;
        }));
        env.define('number?', new PrimitiveProcedure(function(_ref7) {
            var _ref8 = _slicedToArray(_ref7, 1),
                arg = _ref8[0];
            return arg instanceof Number;
        }));
        env.define('procedure?', new PrimitiveProcedure(function(_ref9) {
            var _ref10 = _slicedToArray(_ref9, 1),
                arg = _ref10[0];
            return arg instanceof Procedure || arg instanceof PrimitiveProcedure;
        }));
        env.define('car', new PrimitiveProcedure(function(_ref11) {
            var _ref12 = _slicedToArray(_ref11, 1),
                arg = _ref12[0];
            return arg[0];
        }));
        env.define('cdr', new PrimitiveProcedure(function(_ref13) {
            var _ref14 = _slicedToArray(_ref13, 1),
                arg = _ref14[0];
            return arg.length > 1 ? arg.slice(1) : NULL_VALUE;
        }));
        env.define('cons', new PrimitiveProcedure(function(_ref15) {
            var _ref16 = _slicedToArray(_ref15, 2),
                a = _ref16[0],
                b = _ref16[1];
            return [a].concat(_toConsumableArray(b));
        }));
        env.define('remainder', new PrimitiveProcedure(function(_ref17) {
            var _ref18 = _slicedToArray(_ref17, 2),
                a = _ref18[0],
                b = _ref18[1];
            return a % b;
        }));
        env.define('quote', new PrimitiveProcedure(function(_ref19) {
            var _ref20 = _slicedToArray(_ref19, 1),
                arg = _ref20[0];
            return arg;
        }));
        env.define('begin', new PrimitiveProcedure(function(args) {
            return args[args.length - 1];
        }));
        env.define('equal?', new PrimitiveProcedure(function(_ref21) {
            var _ref22 = _slicedToArray(_ref21, 2),
                a = _ref22[0],
                b = _ref22[1];
            return a == b;
        }));
        env.define('not', new PrimitiveProcedure(function(_ref23) {
            var _ref24 = _slicedToArray(_ref23, 1),
                arg = _ref24[0];
            return !arg;
        }));
        env.define('round', new PrimitiveProcedure(function(_ref25) {
            var _ref26 = _slicedToArray(_ref25, 1),
                arg = _ref26[0];
            return Math.round(arg);
        }));
        env.define('abs', new PrimitiveProcedure(function(_ref27) {
            var _ref28 = _slicedToArray(_ref27, 1),
                arg = _ref28[0];
            return Math.abs(arg);
        }));
        env.define('display', new PrimitiveProcedure(function(_ref29) {
            var _ref30 = _slicedToArray(_ref29, 1),
                arg = _ref30[0];
            return console.log(stringify(arg));
        }));
        env.define('apply', new PrimitiveProcedure(function(_ref31) {
            var _ref32 = _slicedToArray(_ref31, 2),
                proc = _ref32[0],
                args = _ref32[1];
            return proc.call(_this10, args);
        }));
        this.env = env;
    }

    /**
     * Executes the list of expressions and returns the result. If a
     * RuntimeError occurs, it prints the error and returns the null value.
     * @param {Expr[]} expressions
     */
    _createClass(Interpreter, [{
        key: "interpretAll",
        value: function interpretAll(expressions) {
            try {
                var result;
                var _iterator = _createForOfIteratorHelper(expressions),
                    _step;
                try {
                    for (_iterator.s(); !(_step = _iterator.n()).done;) {
                        var expr = _step.value;
                        result = this.interpret(expr, this.env);
                    }
                } catch (err) {
                    _iterator.e(err);
                } finally {
                    _iterator.f();
                }
                return result;
            } catch (error) {
                if (error instanceof RuntimeError) {
                    console.error(error.toString());
                    return NULL_VALUE;
                }
                throw error;
            }
        }

        /**
         * Executes an expression within an environment and returns its value
         * @param {Expr} expr
         * @param {Environment} env
         */
    }, {
        key: "interpret",
        value: function interpret(expr, env) {
            var _this11 = this;
            while (true) {
                if (expr instanceof CallExpr) {
                    var callee = this.interpret(expr.callee, env);
                    var args = expr.args.map(function(arg) {
                        return _this11.interpret(arg, env);
                    });

                    // Eliminate the tail-call of running this procedure by "continuing the interpret loop"
                    // with the procedure's body set as the current expression and the procedure's closure set
                    // as the current environment
                    if (callee instanceof Procedure) {
                        var callEnv = new Environment(callee.declaration.params, args, callee.closure);
                        var _iterator2 = _createForOfIteratorHelper(callee.declaration.body.slice(0, -1)),
                            _step2;
                        try {
                            for (_iterator2.s(); !(_step2 = _iterator2.n()).done;) {
                                var exprInBody = _step2.value;
                                this.interpret(exprInBody, callEnv);
                            }
                        } catch (err) {
                            _iterator2.e(err);
                        } finally {
                            _iterator2.f();
                        }
                        expr = callee.declaration.body[callee.declaration.body.length - 1];
                        env = callEnv;
                        continue;
                    }
                    if (callee instanceof PrimitiveProcedure) {
                        return callee.call(this, args);
                    }
                    throw new RuntimeError('Cannot call ' + stringify(callee));
                }
                if (expr instanceof LiteralExpr) {
                    return expr.value;
                }
                if (expr instanceof SymbolExpr) {
                    return env.get(expr.token.lexeme);
                }
                if (expr instanceof LambdaExpr) {
                    return new Procedure(expr, env);
                }
                if (expr instanceof DefineExpr) {
                    env.define(expr.name.lexeme, this.interpret(expr.value, env));
                    return;
                }
                if (expr instanceof IfExpr) {
                    var condition = this.interpret(expr.condition, env);
                    // Eliminate the tail call of the if branches by "continuing the interpret loop"
                    // with the branch set as the current expression
                    expr = condition != false ? expr.thenBranch : expr.elseBranch;
                    continue;
                }
                if (expr instanceof SetExpr) {
                    return env.set(expr.name.lexeme, this.interpret(expr.value, env));
                }
                if (expr instanceof LetExpr) {
                    var names = expr.bindings.map(function(binding) {
                        return binding.name;
                    });
                    var values = expr.bindings.map(function(binding) {
                        return _this11.interpret(binding.value, env);
                    });
                    var letEnv = new Environment(names, values, env);
                    var _iterator3 = _createForOfIteratorHelper(expr.body.slice(0, -1)),
                        _step3;
                    try {
                        for (_iterator3.s(); !(_step3 = _iterator3.n()).done;) {
                            var _exprInBody = _step3.value;
                            this.interpret(_exprInBody, letEnv);
                        }
                    } catch (err) {
                        _iterator3.e(err);
                    } finally {
                        _iterator3.f();
                    }
                    expr = expr.body[expr.body.length - 1];
                    env = letEnv;
                    continue;
                }
                if (expr instanceof QuoteExpr) {
                    return this.interpret(expr.value, env);
                }
                if (expr instanceof ListExpr) {
                    return expr.items.map(function(value) {
                        return _this11.interpret(value, env);
                    });
                }
                throw new Error('Cannot interpret: ' + expr.constructor.name); // Should be un-reachable
            }
        }
    }]);
    return Interpreter;
}();
/**
 * An Environment stores variables and their values
 */
var Environment = /*#__PURE__*/ function() {
    /**
     *
     * @param {Token | Token[]=} names
     * @param {any[]=} values
     * @param {Environment=} enclosing
     */
    function Environment() {
        var _this12 = this;
        var names = arguments.length > 0 && arguments[0] != undefined ? arguments[0] : [];
        var values = arguments.length > 1 ? arguments[1] : undefined;
        var enclosing = arguments.length > 2 ? arguments[2] : undefined;
        /**
         * @type {Map<string, any>}
         */
        this.values = {};
        if (Array.isArray(names)) {
            names.forEach(function(param, i) {
                _this12.values[param.lexeme] = values[i];
            });
        } else {
            this.values[names.lexeme] = values;
        }
        this.enclosing = enclosing;
    }

    /**
     * Define the value of name in the current environment
     * @param {string} name
     * @param {any} value
     */
    _createClass(Environment, [{
        key: "define",
        value: function define(name, value) {
            this.values[name] = value;
        }

        /**
         * Sets the value of name defined in this environment or an enclosing environment
         * @param {string} name
         * @param {any} value
         */
    }, {
        key: "set",
        value: function set(name, value) {
            if (this.values[name] != undefined) {
                return this.values[name] = value;
            }
            if (this.enclosing) {
                return this.enclosing.set(name, value);
            }
            throw new RuntimeError('Unknown identifier: ' + name);
        }

        /**
         * Gets the value of name from the environment. If the name doesn't
         * exist in this environment, it checks in the enclosing environment.
         * When there is no longer an enclosing environment to check and the
         * name is not found, it throws a RuntimeError.
         * @param {string} name
         */
    }, {
        key: "get",
        value: function get(name) {
            if (this.values[name] != undefined) {
                return this.values[name];
            }
            if (this.enclosing) {
                return this.enclosing.get(name);
            }
            throw new RuntimeError('Unknown identifier: ' + name);
        }
    }]);
    return Environment;
}();
/**
 * A primitive procedure that can be called from the program.
 */
var PrimitiveProcedure = /*#__PURE__*/ function() {
    /**
     *
     * @param {Function} declaration
     */
    function PrimitiveProcedure(declaration) {
        this.declaration = declaration;
    }

    /**
     *
     * @param {Interpreter} _
     * @param {any[]} params
     */
    _createClass(PrimitiveProcedure, [{
        key: "call",
        value: function call(_, params) {
            return this.declaration(params);
        }
    }]);
    return PrimitiveProcedure;
}();
/**
 * A Procedure defined in the program
 */
var Procedure = /*#__PURE__*/ function() {
    /**
     * @param {LambdaExpr} declaration
     * @param {Environment} closure
     */
    function Procedure(declaration, closure) {
        this.declaration = declaration;
        this.closure = closure;
    }

    /**
     * Sets up the environment for the procedure given its
     * arguments and then runs the procedure's body
     * @param {Interpreter} interpreter
     * @param {any[]} args
     * @returns
     */
    _createClass(Procedure, [{
        key: "call",
        value: function call(interpreter, args) {
            var env = new Environment(this.declaration.params, args, this.closure);
            var result;
            var _iterator4 = _createForOfIteratorHelper(this.declaration.body),
                _step4;
            try {
                for (_iterator4.s(); !(_step4 = _iterator4.n()).done;) {
                    var expr = _step4.value;
                    result = interpreter.interpret(expr, env);
                }
            } catch (err) {
                _iterator4.e(err);
            } finally {
                _iterator4.f();
            }
            return result;
        }
    }]);
    return Procedure;
}();
/**
 * A run-time error
 */
var RuntimeError = /*#__PURE__*/ function(_Error2) {
    _inherits(RuntimeError, _Error2);
    var _super10 = _createSuper(RuntimeError);

    function RuntimeError(message) {
        return _super10.call(this, message);
    }
    _createClass(RuntimeError, [{
        key: "toString",
        value: function toString() {
            return 'Error: ' + this.message;
        }
    }]);
    return RuntimeError;
}( /*#__PURE__*/ _wrapNativeSuper(Error));
var interpreter = new Interpreter();


/**
 * Converts the given value to a printable string
 * @param {any} value
 * @returns {string}
 */
function stringify(value) {
    if (value == false) return '#f';
    if (value == true) return '#t';
    if (Array.isArray(value)) return '(' + value.map(stringify).join(' ') + ')';
    if (value instanceof PrimitiveProcedure) return 'PrimitiveProcedure';
    if (value instanceof Procedure) return 'Procedure';
    if (typeof value == 'string') return "" + value;
    if (typeof value == 'number') return "" + value;
    return value;
}

function run(source) {
    var scanner = new Scanner(source);
    var _scanner$scan = scanner.scan(),
        tokens = _scanner$scan.tokens,
        hadError = _scanner$scan.hadError;
    if (hadError) {
        return;
    }
    var parser = new Parser(tokens);
    var expressions = parser.parse();
    var result = interpreter.interpretAll(expressions);
    return stringify(result);
}


// TESTS
// Source: https://github.com/FZSS/scheme/blob/master/tests.scm
var countall = 0;
var countfail = 0;

function test(src, expected) {
    countall++;
    console.log("running: " + JSON.stringify(src) + " ...");
    var val = run(src);
    if (val != expected) {
        countfail++;
        var tv = typeof(val);
        var te = typeof(expected);
        var sv = JSON.stringify(val);
        console.log("FAILED to match <" + sv + " (" + tv + ")" + "> with <" + expected + " (" + te + ")" + ">!");
    }
}


test("10", '10');
test("(+ 137 349)", '486');
test("(- 1000 334)", '666');
test("(* 5 99)", '495');
test("(/ 10 5)", '2');
test("(+ 2.7 10)", '12.7');
test("(+ 21 35 12 7)", '75');
test("(* 25 4 12)", '1200');
test("(+ (* 3 5) (- 10 6))", '19');
test("(+ (* 3 (+ (* 2 4) (+ 3 5))) (+ (- 10 7) 6))", '57');
test(" \
(+ (* 3 \
  (+ (* 2 4) \
     (+ 3 5))) \
(+ (- 10 7) \
  6))", '57');
run('(define size 2)');
test("size", '2');
test("(* 5 size)", 10);
run('(define pi 3.14159)');
run('(define radius 10)');
test("(* pi (* radius radius))", 314.159);
run('(define circumference (* 2 pi radius))');
test("circumference", 62.8318);
run('(define square (lambda (x) (* x x)))');
test("(square 21)", '441');
run('(define and (lambda (x y) (if x y #f)))');
run('(define x 3)');
run('(define y 4)');
test("(and (if (= x 3) #t #f) (if (= y 4) #t #f))", '#t');
run('(define or (lambda (x y) (if x x y)))');
test("(or #f #t)", '#t');
run("\
(define fibonacci \
  (lambda (num) \
    (if (<= num 1) \
        num \
        (+ (fibonacci (- num 1)) (fibonacci (- num 2)))))) \
");
test("(fibonacci 20)", '6765');
test("((lambda (x) (* x 2)) 7)", 14);
test('(define str "hello world") str', 'hello world');
test("(string-length str)", '11');
test('(string-append str " here")', 'hello world here');
test("(define one-through-four (list 1 2 3 4)) one-through-four", '(1 2 3 4)');
test("(car one-through-four)", '1');
test("(car (cdr one-through-four))", '2');
test("(null? (cdr (list)))", '#t');
run(" \
(define map \
  (lambda (proc items) \
    (if (null? items) \
        (list) \
        (cons (proc (car items)) \
              (map proc (cdr items))))))\
");
test("(map square (list 1 2 3))", '(1 4 9)');
run("(define odd? (lambda (x) (= 1 (remainder x 2))))");
run(" \
(define filter \
  (lambda (predicate sequence) \
    (if (null? sequence) \
        (list) \
        (if (predicate (car sequence)) \
            (cons (car sequence) \
                  (filter predicate (cdr sequence))) \
            (filter predicate (cdr sequence)))))) \
");
test("(filter odd? (list 1 2 3 4 5))", '(1 3 5)');
run(" \
(define sum-to \
  (lambda (n acc) \
    (if (= n 0) \
        acc \
        (sum-to (- n 1) (+ n acc))))) \
");
test("(sum-to 10000 0)", '50005000');
run("(define range (lambda (a b) (if (= a b) (quote ()) (cons a (range (+ a 1) b)))))");
test("(range 0 10)", '(0 1 2 3 4 5 6 7 8 9)');
run("(define fact (lambda (n) (if (<= n 1) 1 (* n (fact (- n 1))))))");
test("(fact 100)", '9.33262154439441e+157');
test("(begin (define r 10) (* pi (* r r)))", '314.159');
test("(apply + (list 10 20 30))", '60');
test("(apply map (list fact (list 5 10)))", '(120 3628800)');
run("\
(define x 3) \
(define foo (lambda () (define x 4) x)) \
(define bar (lambda () (set! x 4) x)) \
");
test("(foo)", '4');
test("x", '3');
test("(bar)", '4');
test("x", '4');
run(" \
(define f (lambda (x y) \
  (let ((a (+ 1 (* x y))) \
        (b (- 1 y))) \
    (+ (* x (square a)) \
       (* y b) \
       (* a b))))) \
");
test("(f 3 4)", '456');
run(" \
(define gen (lambda () \
(let ((n 0)) \
  (lambda () \
    (set! n (+ n 1)) \
    n))))\
");
run("(define next (gen))");
run("(display (next))");
run("(display (next))");
run("(display (next))");
test("(* 2 (* 8 4)) ; end of line comment", 64);
test("((lambda args (car (car args))) (quote (1 2 3 4 5)))", '1');
test("((lambda args (car (car args))) '(1 2 3 4 5))", '1');

/*
dorun(" \
 (define-macro and (lambda args \
   (if (null? args) #t \
       (if (= (length args) 1) (car args) \
           `(if ,(car args) (and ,@(cdr args)) #f))))) \
");
*/
console.log("out of " + countall + " tests, " + countfail + " failed");

// REPL
/*
interpreter = new Interpreter();
const repl = require('repl');
repl.start({
  prompt: 'jscheme> ',
  eval: (input, context, filename, callback) => {
    callback(null, run(input));
  }
});
*/