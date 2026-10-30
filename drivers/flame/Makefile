OBJ:=flame_app

OBJS+=main.c

CC:=arm-linux-gnueabihf-gcc

$(OBJ):$(OBJS)
	$(CC) $^ -o $@ -lm
	cp $(OBJ) ~/nfs/rootfs

.PHONY:
clean:
	rm $(OBJ)
