/**
 * Mark and Sweep Garbage Collection technique.
 * MIT Style License
 * by Dmitry Soshnikov
 */

// This diff describes the simplest version of mark and sweep
// GC in order to understand the basic idea. In real practice the
// implementation can be much tricker and optimized.

// ----------------------------------------------------------
// Objects allocation and the "roots"
// ----------------------------------------------------------

// For simplicity represent the "heap" (the memory where
// our objects are stored) as a plain array.
var heap = [];

// Let's do some heap and pointers manipulations, allocate some
// objects and put some references from one objects to another.

// Allocate "a" object.
var a = {name: 'a'};
heap.push(a);

// Side note: the "Roots".

// A "Root" is a place from where we start our GC analysis.
// For simplicity we assume the root is the first element of the
// heap array. To make the GC we should follow all reachable pointers
// starting from the root. Note: in real practice of course there are
// many roots (e.g. all global variables, or at lower level -- all objects
// directly reachable from registers and stack frames).
function root() {
  return heap[0];
}

// ----------------------------------------------------------
// Reachable objects
// ----------------------------------------------------------

// Object "b" is allocated and is reachable from "a", so the chain is:
// root -> a -> b
var b = {name: 'b'};
heap.push(b);
a.b = b;

// Object "c" is allocated and is also reachable from "a": root -> a -> c
var c = {name: 'c'};
heap.push(c);
a.c = c;

// However, later it's reference from "a" is removed:
delete a.c;
// Now "c" is a candidate for GC.

// Object "d" is allocated and is reachable from "b" (which in trun is
// reachable from "a": root -> a -> b -> d
var d = {name: 'd'};
heap.push(d);
b.d = d;

// But then the "b" reference is removed from "a".
delete a.b;
// This means that "d" still has the reference to it from "b", but it's
// not reachable (since the "b" itself is not reachable anymore).
// root -> a --X--> b -> d

// Notice the important point: that an object has some references to it
// doesn't mean it cannot be GC'ed. The creteria is "reachability", but not the
// reference counting here.

// After these manipulations the heap still contains four objects:
// [{a}, {b}, {c}, {d}], but only the "a" object is reachable
// (starting from the root).

// ----------------------------------------------------------
// Mark and Sweep GC
// ----------------------------------------------------------

// Mark and Sweep GC works in two phases
// (yes, not surprizingly, the Mark phase, and the Sweep phase).
function gc() {
  // In Mark phase we trace all reachable objects.
  mark();
  // And in the sweep phase collect the garbage.
  sweep();
}

// ----------------------------------------------------------
// Mark phase.
// ----------------------------------------------------------

// The "mark" phase traverses all reachable objects starting from
// the root and _mark_ them explicitly as reachable (by just setting
// the so-called "mark bit" to 1).
function mark() {
  // Initialy in our "todo" list (the list of _reachable_
  // objects to analyze) is only the root object.
  var todo = [root()];
  // while we have some objects to follow...
  while (todo.length) {
    // pick the next object to process
    var o = todo.pop();
    // If the object is not marked yet:
    if (!o.__markBit__) {
      // then we mark it
      o.__markBit__ = 1;
      // and add _all reachable_ pointers from
      // this "o" object to our todo list.
      for (var k in o) if (typeof o[k] == 'object') {
        todo.push(o[k]);
      }
    }
  }
}

// Note: traversing of all reachable objects could be
// done recursively of course in the example above. Or at lower
// level by traversign "reverse-pointers" to find the way back
// after we analyzed one rout of the objects graph.

// ----------------------------------------------------------
// Sweep phase.
// ----------------------------------------------------------

// After we have all reachable objects marked, we can start
// the sweep phase. We simply traverse the heap and move
// all unmarked (that is, unreachable objects) to the free list.
function sweep() {
  heap = heap.filter(function(o) {
    // If the object is marked, keep it and reset the mark bit
    // back to zero (for future GC cycles).
    if (o.__markBit__ == 1) {
      o.__markBit__ = 0;
      return true;
    } else {
      // Remove garbage object ("move to free list")
      return false;
    }
  });
}

// Before GC: [{a}, {b}, {c}, {d}]
println('Heap before GC:', heap);

// Run GC
gc();

// After GC: [{a}]
println('Heap after GC:', heap);