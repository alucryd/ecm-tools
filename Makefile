DEPS = banner.h common.h version.h

OBJ = ecm.o

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

bin2ecm: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $<

.PHONY: install

install:
	install -dm 755 $(DESTDIR)/usr/bin
	install -m 755 bin2ecm $(DESTDIR)/usr/bin/
	ln -s bin2ecm $(DESTDIR)/usr/bin/ecm2bin

.PHONY: clean

clean:
	rm ecm.o bin2ecm
