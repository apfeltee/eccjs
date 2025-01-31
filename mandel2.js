
function do_add(x, y) {
    return x + y;
}

function do_sub(x, y) {
    return x - y;
}

function do_mul(x, y) {
    var t = x * y;
    return (t + 8192 / 2) / 8192;
}

function do_div(x, y) {
    var t = x * 8192;
    return (t + y / 2) / y;
}

function of_int(x) {
    return x * 8192;
}

function iter(n, a, b, xn, yn) {
    if (n == 100)
        return 1;
    var xn2 = do_mul(xn, xn);
    var yn2 = do_mul(yn, yn);
    if (do_add(xn2, yn2) > of_int(4))
        return 0;
    return iter(n + 1, a, b, do_add(do_sub(xn2, yn2), a), do_add(do_mul(of_int(2), do_mul(xn, yn)), b));
}

function inside(x, y) {
    return iter(0, x, y, of_int(0), of_int(0));
}

function run() {
    var steps = 30;
    var xmin = of_int(-2);
    var xmax = of_int(1);
    var deltax = do_div(do_sub(xmax, xmin), of_int(2 * steps));
    var ymin = of_int(-1);
    var ymax = of_int(1);
    var deltay = do_div(do_sub(ymax, ymin), of_int(steps));
    for (var i = 0; i < steps; i++) {
        var y = do_add(ymin, do_mul(of_int(i), deltay));
        for (var j = 0; j < 2 * steps; j++) {
            var x = do_add(xmin, do_mul(of_int(j), deltax));
            if (inside(x, y)) {
                print("#");
            } else {
                print("-");
            }
        }
        print("\n");
    }
}

run();