#pragma once
// Qt defines `signals` as an access-specifier macro that expands to
// `public QT_ANNOTATE_ACCESS_SPECIFIER(qt_signal)`.  This conflicts with
// the `signals` data member in socketspy::dbc::Message.
//
// This header is intended to be included from .cpp translation units AFTER
// all Qt headers.  It permanently undefines `signals` for the rest of the
// translation unit, which is safe in .cpp files because:
//   - The `signals:` access specifier is only needed inside class bodies in
//     .h files (handled by the normal Qt headers).
//   - .cpp files never contain `signals:` — they use `emit` / `connect()`.
#ifdef signals
#  undef signals
#endif
#include "dbc_types.h"
#include "dbc_parser.h"
#include "dbc_signal.h"
