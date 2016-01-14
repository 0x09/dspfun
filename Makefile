PROJS = spec motion applybasis zoom

.PHONY: projs $(PROJS) clean

projs: $(PROJS)

$(PROJS):
	$(MAKE) -C $@

clean:
	for dir in $(PROJS); do \
		$(MAKE) -C $$dir $@; \
	done