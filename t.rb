
src=<<__eos__
static int arrayobjfn_valueIsArray(eccvalue_t value);
static uint32_t arrayobjfn_valueArrayLength(eccvalue_t value);
static uint32_t arrayobjfn_objectLength(eccstate_t *context, eccobject_t *object);
static void arrayobjfn_objectResize(eccstate_t *context, eccobject_t *object, uint32_t length);
static void arrayobjfn_valueAppendFromElement(eccstate_t *context, eccvalue_t value, eccobject_t *object, uint32_t *element);
static inline int arrayobjfn_gcd(int m, int n);
static inline void arrayobjfn_rotate(eccobject_t *object, eccstate_t *context, uint32_t first, uint32_t half, uint32_t last);
static inline int arrayobjfn_compare(eccarraycomparestate_t *cmp, eccvalue_t left, eccvalue_t right);
static inline uint32_t arrayobjfn_search(eccobject_t *object, eccarraycomparestate_t *cmp, uint32_t first, uint32_t last, eccvalue_t right);
static inline void arrayobjfn_merge(eccobject_t *object, eccarraycomparestate_t *cmp, uint32_t first, uint32_t pivot, uint32_t last, uint32_t len1, uint32_t len2);
static void arrayobjfn_sortAndMerge(eccobject_t *object, eccarraycomparestate_t *cmp, uint32_t first, uint32_t last);
static void arrayobjfn_sortInPlace(eccstate_t *context, eccobject_t *object, eccobjscriptfunction_t *function, int first, int last);
void nsarrayfn_setup(void);
void nsarrayfn_teardown(void);
eccobject_t *nsarrayfn_create(void);
eccobject_t *nsarrayfn_createSized(uint32_t size);

__eos__

tmp = []
src.each_line do |l|
  l.strip!
  next if l.empty?
  m = l.match(/\b(?<name>\w+)\b\(/)
  name = m['name']
  next if name.match?(/nsarrayfn/)

  actual = name.gsub(/^arrayobjfn_/, '')
  tmp.push(actual)

end

print('\barrayobjfn_(' + tmp.join('|') + ')\b')

