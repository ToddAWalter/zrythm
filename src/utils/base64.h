// SPDX-FileCopyrightText: © 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#pragma once

#include <QByteArray>

namespace zrythm::utils::base64
{
inline QByteArray
encode (const QByteArray &input)
{
  return input.toBase64 ();
}

inline QByteArray
decode (const QByteArray &input)
{
  return QByteArray::fromBase64 (input);
}
}; // namespace zrythm::utils::base64