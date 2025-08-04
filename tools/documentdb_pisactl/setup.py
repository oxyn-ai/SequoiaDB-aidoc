#!/usr/bin/env python3

from setuptools import setup, find_packages

setup(
    name="documentdb-pisactl",
    version="1.0.0",
    description="DocumentDB-integrated PISA control tools for automated index management",
    author="DocumentDB Team",
    author_email="documentdb@microsoft.com",
    packages=find_packages(where="src"),
    package_dir={"": "src"},
    python_requires=">=3.8",
    install_requires=[
        "psycopg2-binary>=2.9.0",
        "pydantic>=1.8.0",
        "ruamel.yaml>=0.17.0",
        "click>=8.0.0",
        "rich>=10.0.0",
        "asyncio-mqtt>=0.11.0",
        "schedule>=1.1.0",
    ],
    entry_points={
        "console_scripts": [
            "documentdb-pisactl=documentdb_pisactl.cmd:main",
            "documentdb-pisa-monitor=documentdb_pisactl.monitor:main",
            "documentdb-pisa-scheduler=documentdb_pisactl.scheduler:main",
        ],
    },
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: Apache Software License",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
    ],
)
