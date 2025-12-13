.PHONY: build install all clean ctest

all: ctest build install

install:
	python setup.py build_ext --inplace --force
	pip install -e .

ctest:
	$(MAKE) -C src test

test:
	pytest tests/ -s

clean:
	rm -rf build *.so *.pyd *.c *.cpp
