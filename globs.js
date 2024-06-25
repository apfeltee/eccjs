function globals() {
    return this;
}

var safe_stringify = function(obj) {
    var cache = [];
    var r = JSON.stringify(obj, function(key, value) {
        try {
            if (value !== null) {
                if (typeof(value) === 'object') {
                    if (cache.indexOf(value) !== -1) {
                        // Circular reference found, discard key
                        return;
                    }
                    // Store value in our collection
                    cache.push(value);
                } else if (typeof(value) === 'function') {
                    return value.toString();
                }
            }
        } catch (exc) {
            console.log("exception during stringify: " + exc);
        }
        return value;
    });
    cache = null; // Enable garbage collection
    return r;
};

function pp_call(thing, fnc) {
    var t = typeof(thing);
    if (t == "string") {
        fnc('"');
        fnc(thing);
        fnc('"');
    } else if (t == "array") {
        fnc("[");
        fnc("\n");
        for (var i = 0; i < thing.length; i++) {
            fnc("  [");
            fnc(i.toString());
            fnc("]: ");
            if (thing[i] == thing) {
                fnc("<recursion>");
            } else {
                pp_call(thing[i], fnc);
            }
            fnc(",");
            fnc("\n");
        }
        fnc("]");
        fnc("\n");
    } else if (t == "object") {
        var ents = Object.keys(thing);
        fnc("{");
        fnc("\n");

        for (var i = 0; i < ents.length; i++) {
            var key = ents[i];
            var v = thing[key];
            fnc(key.toString());
            fnc(": ");
            if (v == thing) {
                fnc("<recursion>");
            } else {
                pp_call(v, fnc);
            }
            fnc(",");
            fnc("\n");
        }
        fnc("\n");
        fnc("}");
        fnc("\n");
    } else {
        if (thing == undefined) {
            fnc("undefined");
        } else if (thing == null) {
            fnc("null");
        } else {
            fnc(thing.toString());
        }
    }
}

function pp(thing) {
    var buf = [];
    var recv = function(v) {
        if (v == "\n") {
            console.log(buf.join(""));
            buf = [];
        } else {
            buf.push(v);
        };
    };
    pp_call(thing, recv);
    if (buf.length != 0) {
        console.log(buf.join(""));
    }
}

function dump(name, thing) {
    console.log("contents of [" + name + "]:");
    //pp(thing);
    console.log(safe_stringify(thing));
}

var props;
var keys = []
props = Object.getOwnPropertyNames(globals());
try {
    keys = Object.keys(Object.getPrototypeOf(globals()))
} catch (e) {
    console.log("getPrototypeOf(this) failed: ", e.message);
}

dump("this", globals());
dump("props", props);
dump("keys", keys);