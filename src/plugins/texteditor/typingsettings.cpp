/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "typingsettings.h"

#include <utils/settingsutils.h>
#include <QTextCursor>
#include <QTextDocument>

static const char autoIndentKey[] = "AutoIndent";
static const char tabKeyBehaviorKey[] = "TabKeyBehavior";
static const char smartBackspaceBehaviorKey[] = "SmartBackspaceBehavior";
static const char groupPostfix[] = "TypingSettings";


namespace TextEditor {

TypingSettings::TypingSettings():
    //OPENMV-DIFF//
    //m_autoIndent(true),
    //OPENMV-DIFF//
    m_autoIndent(false),
    //OPENMV-DIFF//
    m_tabKeyBehavior(TabNeverIndents),
    m_smartBackspaceBehavior(BackspaceNeverIndents)
{
}

void TypingSettings::toSettings(const QString &category, QSettings *s) const
{
    Utils::toSettings(QLatin1String(groupPostfix), category, s, this);
}

void TypingSettings::fromSettings(const QString &category, const QSettings *s)
{
    *this = TypingSettings(); // Assign defaults
    Utils::fromSettings(QLatin1String(groupPostfix), category, s, this);
}

void TypingSettings::toMap(const QString &prefix, QVariantMap *map) const
{
    map->insert(prefix + QLatin1String(autoIndentKey), m_autoIndent);
    map->insert(prefix + QLatin1String(tabKeyBehaviorKey), m_tabKeyBehavior);
    map->insert(prefix + QLatin1String(smartBackspaceBehaviorKey), m_smartBackspaceBehavior);
}

void TypingSettings::fromMap(const QString &prefix, const QVariantMap &map)
{
    m_autoIndent = map.value(prefix + QLatin1String(autoIndentKey), m_autoIndent).toBool();
    //OPENMV-DIFF//
    m_autoIndent = false;
    //OPENMV-DIFF//
    m_tabKeyBehavior = (TabKeyBehavior)
        map.value(prefix + QLatin1String(tabKeyBehaviorKey), m_tabKeyBehavior).toInt();
    //OPENMV-DIFF//
    m_tabKeyBehavior = TabNeverIndents;
    //OPENMV-DIFF//
    m_smartBackspaceBehavior = (SmartBackspaceBehavior)
        map.value(prefix + QLatin1String(smartBackspaceBehaviorKey),
                  m_smartBackspaceBehavior).toInt();

}

bool TypingSettings::equals(const TypingSettings &ts) const
{
    return m_autoIndent == ts.m_autoIndent
        && m_tabKeyBehavior == ts.m_tabKeyBehavior
        && m_smartBackspaceBehavior == ts.m_smartBackspaceBehavior;
}

bool TypingSettings::tabShouldIndent(const QTextDocument *document, QTextCursor cursor, int *suggestedPosition) const
{
    if (m_tabKeyBehavior == TabNeverIndents)
        return false;
    QTextCursor tc = cursor;
    if (suggestedPosition)
        *suggestedPosition = tc.position(); // At least suggest original position
    tc.movePosition(QTextCursor::StartOfLine);
    if (tc.atBlockEnd()) // cursor was on a blank line
        return true;
    if (document->characterAt(tc.position()).isSpace()) {
        tc.movePosition(QTextCursor::WordRight);
        if (tc.positionInBlock() >= cursor.positionInBlock()) {
            if (suggestedPosition)
                *suggestedPosition = tc.position(); // Suggest position after whitespace
            if (m_tabKeyBehavior == TabLeadingWhitespaceIndents)
                return true;
        }
    }
    return (m_tabKeyBehavior == TabAlwaysIndents);
}


} // namespace TextEditor
