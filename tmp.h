

#define mac_stepiteration(value, nextOps, then) \
    { \
        uint32_t indices[3]; \
        ecc_mempool_getindices(indices); \
        value = opmac_next(); \
        if(context->breaker && --context->breaker) \
        { \
            /* return breaker */ \
            if(--context->breaker) \
            { \
                return value; \
            } \
            else \
            { \
                then; \
            } \
        } \
        else \
        { \
            ecc_mempool_collectunreferencedfromindices(indices); \
            context->ops = nextOps; \
        } \
    }

void stuff()
{
            for(index = 0; index < count; ++index)
            {
                element = object->hmapitemitems + index;
                if(element->hmapitemvalue.check != 1 || (element->hmapitemvalue.flags & ECC_VALFLAG_HIDDEN))
                {
                    continue;
                }
                if(object != target.data.object && &element->hmapitemvalue != ecc_object_element(target.data.object, index, 0))
                {
                    continue;
                }
                ecc_strbuf_beginappend(&chars);
                ecc_strbuf_append(&chars, "%d", index);
                key = ecc_strbuf_endappend(&chars);
                ecc_oper_replacerefvalue(ref, key);-
                mac_stepiteration(value, startOps, { break; });
            }
}
