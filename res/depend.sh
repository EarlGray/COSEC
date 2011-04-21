#! /bin/bash


MKNEW=Makefile.new
MKOLD=Makefile.bak
DEPSTR="\#\#\# Dependencies"
AS_RULE="\@echo \"AS \$<\" && \$(AS) \$(ASFLAGS) -c \$< -o \$@ && 	\@echo \"\""
C_RULE="\@echo \"CC \$<\" && \$(CC) \$(CCFLAGS) -c \$< -o \$@ && 	@echo \"\""

sed "/$DEPSTR/q" < Makefile > $MKNEW

echo -e "$DEPSTR" >> $MKNEW
echo -n "objs =" >> $MKNEW

# to make kstart.asm first in list
for i in *.S ; do 
    echo -n " \$(BUILD)/${i%.*}.o"
done >> $MKNEW

for i in *.c ; do 
    echo -n " \$(BUILD)/${i%.*}.o"
done >> $MKNEW

echo >> $MKNEW
echo >> $MKNEW

for i in *.[cS] ; do 
    cpp -M -I. $i -MT "\$(BUILD)/${i%.*}.o"; 
    echo -e "\t"
    case ${i%*.} in
        "c") 
            echo $C_RULE
            ;;
        "S") 
            echo $AS_RULE
            ;;
        *) echo "Can't recognize file extension ${i%*.}" > 2 ;;
    esac
done >> $MKNEW

mv Makefile $MKOLD
mv $MKNEW Makefile
