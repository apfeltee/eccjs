
function add(i, n) {
    println("args=", [i, n]);
    return i + n;
}

function callme(i) {
    var v = add(i, 1);
    //var v = i + 1
    println("****** callme() being called? i=", i, ", v=", v);
    return v;
}

var a =[22, 33, 44]
println("before: a=", a)
var na = a.map(callme)

println("******continuing******")
println("after: na=", na, ", a=", a)
println("******end of script********");
