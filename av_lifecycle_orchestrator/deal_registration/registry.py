import importlib
import pkgutil
import logging
from typing import Type

from .base import BaseDealRegistrationHandler

logger = logging.getLogger(__name__)

_REGISTRY: dict[str, Type[BaseDealRegistrationHandler]] = {}


def register_handler(name: str):
    """Decorator that registers a deal registration handler class under the given manufacturer name."""

    def decorator(cls: Type[BaseDealRegistrationHandler]):
        if name in _REGISTRY:
            logger.warning(
                f"Handler for '{name}' already registered ({_REGISTRY[name].__name__}), "
                f"overwriting with {cls.__name__}"
            )
        _REGISTRY[name] = cls
        return cls

    return decorator


def get_handler(manufacturer: str) -> BaseDealRegistrationHandler:
    """Instantiate and return the handler for the given manufacturer.

    Raises:
        KeyError: If no handler is registered for the manufacturer.
    """
    if manufacturer not in _REGISTRY:
        raise KeyError(
            f"No deal registration handler registered for '{manufacturer}'. "
            f"Available: {list(_REGISTRY.keys())}"
        )
    return _REGISTRY[manufacturer]()


def list_handlers() -> list[str]:
    """Return a sorted list of registered manufacturer names."""
    return sorted(_REGISTRY.keys())


def _auto_import_handlers() -> None:
    """Auto-import all handler modules in the handlers/ sub-package."""
    from . import handlers

    package_path = handlers.__path__
    for importer, module_name, is_pkg in pkgutil.iter_modules(package_path):
        full_module_name = f"{handlers.__name__}.{module_name}"
        try:
            importlib.import_module(full_module_name)
        except Exception:
            logger.exception(f"Failed to import handler module: {full_module_name}")


_auto_import_handlers()
