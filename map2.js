
var outerone = 1;

function otherthing(n) {
    return n + outerone;
}

function add(n) {
    return n + [111, 222, 333].map(otherthing)[0];
}

function dothing(i) {
    var newv = add(i);
    return newv;
}

var a = [44, 55, 66, 77]
println("before: ", a)
var na = a.map(dothing).map(dothing)
println("after: ", na)
println("--- all done ----")
