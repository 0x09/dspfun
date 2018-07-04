PROJS = spec motion applybasis zoom scan

.PHONY: projs $(PROJS) clean

projs: $(PROJS)

$(PROJS):
	$(MAKE) -C $@

clean:
	for dir in $(PROJS); do \
		$(MAKE) -C $$dir $@; \
	done