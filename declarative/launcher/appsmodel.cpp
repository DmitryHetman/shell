/****************************************************************************
 * This file is part of Hawaii Shell.
 *
 * Copyright (C) 2015-2016 Pier Luigi Fiorini
 *
 * Author(s):
 *    Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
 *
 * $BEGIN_LICENSE:GPL3+$
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * $END_LICENSE$
 ***************************************************************************/

#include "appsmodel.h"

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusInterface>
#include <QtGui/QIcon>

#include <qt5xdg/xdgmenu.h>
#include <qt5xdg/xmlhelper.h>

#include "application.h"

AppsModel::AppsModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

ApplicationManager *AppsModel::applicationManager() const { return m_appMan; }

void AppsModel::setApplicationManager(ApplicationManager *appMan)
{
    if (m_appMan == appMan)
        return;

    beginResetModel();
    m_appMan = appMan;
    m_apps = appMan->applications();
    endResetModel();

    Q_EMIT applicationManagerChanged();
}

QHash<int, QByteArray> AppsModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(Qt::DisplayRole, "display");
    roles.insert(Qt::DecorationRole, "decoration");
    roles.insert(NameRole, "name");
    roles.insert(GenericNameRole, "genericName");
    roles.insert(CommentRole, "comment");
    roles.insert(IconNameRole, "iconName");
    roles.insert(FilterInfoRole, "filterInfo");
    roles.insert(RunningRole, "running");
    roles.insert(CategoriesRole, "categories");
    return roles;
}

int AppsModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_apps.count();
}

QVariant AppsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    Application *app = m_apps[index.row()];

    switch (role) {
    case Qt::DecorationRole:
        return QIcon::fromTheme(app->iconName());
    case Qt::DisplayRole:
    case NameRole:
        return app->name();
    case GenericNameRole:
        return app->genericName();
    case CommentRole:
        return app->comment();
    case IconNameRole:
        return app->iconName();
    case AppIdRole:
        return app->appId();
    case FilterInfoRole:
        return QString(app->name() + app->comment());
    case RunningRole:
        return app->isRunning();
    case CategoriesRole:
        return app->categories();
    default:
        break;
    }

    return QVariant();
}
