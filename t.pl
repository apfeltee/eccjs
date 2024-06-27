
#define 

(self->ops[index].native == ECCNSOperand.createLocalRef) THEN
    ECCNSOperand.getParentSlotRef
    ELSE (
        (self->ops[index].native == ECCNSOperand.getLocalRefOrNull) THEN
            ECCNSOperand.getParentSlotRef
        ELSE (
            (self->ops[index].native == ECCNSOperand.getLocalRef) THEN
                ECCNSOperand.getParentSlotRef
            ELSE (
                (self->ops[index].native == ECCNSOperand.getLocal) THEN
                    ECCNSOperand.getParentSlot
                ELSE (
                    (self->ops[index].native == ECCNSOperand.setLocal) THEN
                        ECCNSOperand.setParentSlot
                    ELSE (
                        (self->ops[index].native == ECCNSOperand.deleteLocal) THEN
                            ECCNSOperand.deleteParentSlot
                        ELSE
                            NULL
                    )
                )
            )
        )
    )
