PROJS = spec motion applybasis zoom scan

.PHONY: projs $(PROJS) clean install uninstall

projs: $(PROJS)

$(PROJS):
	$(MAKE) -C $@

clean:
	for dir in $(PROJS); do \
		$(MAKE) -C $$dir $@; \
	done

install:
	for dir in $(PROJS); do \
		$(MAKE) -C $$dir $@; \
	done

uninstall:
	for dir in $(PROJS); do \
		$(MAKE) -C $$dir $@; \
	done
