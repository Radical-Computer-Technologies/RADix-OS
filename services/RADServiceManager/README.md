# RADServiceManager

RADServiceManager is the planned first userspace process for RADix-OS service
startup. The kernel currently exposes a small boot-service registry and overlay
configuration API so early trusted builds can start services deterministically.

Longer term, service launch policy belongs here: priority, restart behavior,
dependency ordering, and user/session service ownership.
