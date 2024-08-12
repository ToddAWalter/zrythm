// SPDX-FileCopyrightText: © 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "zrythm-config.h"

#include "zrythm_app.h"

/** This is declared extern in zrythm_app.h. */
std::unique_ptr<ZrythmApp, ZrythmAppDeleter> zrythm_app;
