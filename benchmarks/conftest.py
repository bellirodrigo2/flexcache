"""
Pytest configuration for benchmarks.
"""

import pytest


def pytest_configure(config):
    """Add custom markers."""
    config.addinivalue_line("markers", "benchmark: mark test as a benchmark")
    config.addinivalue_line("markers", "unit: mark test as a unit test")


def pytest_collection_modifyitems(config, items):
    """
    Auto-mark tests based on class/function names.
    Tests in TestBenchmark* classes get 'benchmark' marker.
    Tests in TestFunctional* classes get 'unit' marker.
    """
    for item in items:
        # Get the class name if exists
        cls_name = item.cls.__name__ if item.cls else ""

        if cls_name.startswith("TestBenchmark"):
            item.add_marker(pytest.mark.benchmark)
        elif cls_name.startswith("TestFunctional"):
            item.add_marker(pytest.mark.unit)


def pytest_addoption(parser):
    """Add custom command line options."""
    parser.addoption(
        "--benchmark-output-dir",
        action="store",
        default="benchmark_results",
        help="Directory for benchmark output files",
    )


@pytest.fixture(scope="session")
def benchmark_output_dir(request):
    """Get benchmark output directory."""
    return request.config.getoption("--benchmark-output-dir")
