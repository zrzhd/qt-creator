/****************************************************************************
**
** Copyright (C) 2016 Lorenz Haas
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

#ifndef BEAUTIFIER_UNCRUSTIFY_H
#define BEAUTIFIER_UNCRUSTIFY_H

#include "../beautifierabstracttool.h"
#include "../command.h"

QT_FORWARD_DECLARE_CLASS(QAction)

namespace Beautifier {
namespace Internal {

class BeautifierPlugin;

namespace Uncrustify {

class UncrustifySettings;

class Uncrustify : public BeautifierAbstractTool
{
    Q_OBJECT

public:
    explicit Uncrustify(BeautifierPlugin *parent = 0);
    virtual ~Uncrustify();
    bool initialize() override;
    void updateActions(Core::IEditor *editor) override;
    QList<QObject *> autoReleaseObjects() override;

private slots:
    void formatFile();
    void formatSelectedText();

private:
    BeautifierPlugin *m_beautifierPlugin;
    QAction *m_formatFile;
    QAction *m_formatRange;
    UncrustifySettings *m_settings;
    QString configurationFile() const;
    Command command(const QString &cfgFile, bool fragment = false) const;
};

} // namespace Uncrustify
} // namespace Internal
} // namespace Beautifier

#endif // BEAUTIFIER_UNCRUSTIFY_H