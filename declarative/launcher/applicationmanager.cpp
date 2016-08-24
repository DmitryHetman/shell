#include "applicationmanager.h"

#include <QtCore/QFileInfo>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusInterface>
#include <QtGui/QIcon>
#include <qt5xdg/xdgmenu.h>
#include <qt5xdg/xmlhelper.h>

#include "application.h"
#include "usagetracker.h"

Q_LOGGING_CATEGORY(APPLICATION_MANAGER, "hawaii.qml.launcher.applicationmanager")

bool appLessThan(Application *e1, Application *e2)
{
    return e1->name() < e2->name();
}

ApplicationManager::ApplicationManager(QObject *parent)
    : QObject(parent)
{
    m_settings = new QGSettings(QStringLiteral("org.hawaiios.desktop.panel"),
                                QStringLiteral("/org/hawaiios/desktop/panel/"), this);

    refresh();
}

Application *ApplicationManager::getApplication(const QString &appId)
{
    Application *app = m_apps[appId];

    if (app != nullptr)
        return app;

    app = new Application(appId, QStringList(), this);
    m_apps[app->appId()] = app;

    QObject::connect(app, &Application::launched, [=]() { Q_EMIT applicationLaunched(app); });
    QObject::connect(app, &Application::pinnedChanged, [=]() {
        // Currently pinned launchers
        QStringList pinnedLaunchers =
            m_settings->value(QStringLiteral("pinnedLaunchers")).toStringList();

        // Add or remove from the pinned launchers
        if (app->isPinned()) {
            if (!pinnedLaunchers.contains(app->appId()))
                pinnedLaunchers.append(app->appId());
        } else {
            pinnedLaunchers.removeOne(app->appId());
        }
        m_settings->setValue(QStringLiteral("pinnedLaunchers"), pinnedLaunchers);
    });

    Q_EMIT applicationAdded(app);

    return app;
}

void ApplicationManager::launch(const QString &appId)
{
    getApplication(appId)->launch();
}
void ApplicationManager::quit(const QString &appId)
{
    m_appMan->quit(appId);
}

QList<Application *> ApplicationManager::applications() const
{
    QList<Application *> apps;

    for (Application *app : m_apps) {
        if (app->categories().count() > 0 && app->isValid())
            apps.append(app);
    }

    qSort(apps.begin(), apps.end(), appLessThan);

    return apps;
}

QList<Application *> ApplicationManager::pinnedApps() const
{
    // We have to look up the pinnedLaunchers list instead of just getting all pinned
    // apps because the apps map isn't sorted
    QList<Application *> apps;
    QStringList pinnedLaunchers = m_settings->value("pinnedLaunchers").toStringList();

    for (Application *app : m_apps) {
        if (pinnedLaunchers.contains(app->appId()))
            apps.append(app);
    }

    return apps;
}

GreenIsland::Server::ApplicationManager *ApplicationManager::applicationManager() const
{
    return m_appMan;
}

void ApplicationManager::setApplicationManager(GreenIsland::Server::ApplicationManager *appMan)
{
    if (m_appMan == appMan)
        return;

    if (m_appMan != nullptr) {
        disconnect(m_appMan, &GreenIsland::Server::ApplicationManager::applicationAdded, this,
                   &ApplicationManager::handleApplicationAdded);
        disconnect(m_appMan, &GreenIsland::Server::ApplicationManager::applicationRemoved, this,
                   &ApplicationManager::handleApplicationRemoved);
        disconnect(m_appMan, &GreenIsland::Server::ApplicationManager::applicationFocused, this,
                   &ApplicationManager::handleApplicationFocused);
    }

    m_appMan = appMan;
    Q_EMIT applicationManagerChanged();

    if (appMan != nullptr) {
        connect(appMan, &GreenIsland::Server::ApplicationManager::applicationAdded, this,
                &ApplicationManager::handleApplicationAdded);
        connect(appMan, &GreenIsland::Server::ApplicationManager::applicationRemoved, this,
                &ApplicationManager::handleApplicationRemoved);
        connect(appMan, &GreenIsland::Server::ApplicationManager::applicationFocused, this,
                &ApplicationManager::handleApplicationFocused);
    }
}

void ApplicationManager::handleApplicationAdded(QString appId, pid_t pid)
{
    appId = DesktopFile::canonicalAppId(appId);

    Application *app = getApplication(appId);
    app->setState(Application::Running);
    app->m_pids.insert(pid);

    if (!app->desktopFile()->noDisplay())
        UsageTracker::instance()->applicationLaunched(appId);
}

void ApplicationManager::handleApplicationRemoved(QString appId, pid_t pid)
{
    appId = DesktopFile::canonicalAppId(appId);

    UsageTracker::instance()->applicationFocused(QString());

    Application *app = getApplication(appId);

    app->m_pids.remove(pid);

    if (app->m_pids.count() == 0) {
        app->setState(Application::NotRunning);
    }
}

void ApplicationManager::handleApplicationFocused(QString appId)
{
    appId = DesktopFile::canonicalAppId(appId);

    Application *app = getApplication(appId);

    app->setActive(true);

    for (Application *app : m_apps) {
        if (app->appId() != appId)
            app->setActive(false);
    }

    if (!app->desktopFile()->noDisplay())
        UsageTracker::instance()->applicationFocused(appId);
    else
        UsageTracker::instance()->applicationFocused(QString());
}

void ApplicationManager::refresh()
{
    qDeleteAll(m_apps);
    m_apps.clear();

    XdgMenu xdgMenu;
    xdgMenu.setLogDir("/tmp/");
    xdgMenu.setEnvironments(QStringList() << QStringLiteral("Hawaii")
                                          << QStringLiteral("X-Hawaii"));

    const QString menuFileName = XdgMenu::getMenuFileName();

    qCDebug(APPLICATION_MANAGER) << "Menu file name:" << menuFileName;
    if (!xdgMenu.read(menuFileName)) {
        qCWarning(APPLICATION_MANAGER, "Failed to read menu \"%s\": %s", qPrintable(menuFileName),
                  qPrintable(xdgMenu.errorString()));
        return;
    }

    QDomElement xml = xdgMenu.xml().documentElement();

    DomElementIterator it(xml, QString());
    while (it.hasNext()) {
        QDomElement xml = it.next();

        if (xml.tagName() == QStringLiteral("Menu")) {
            QString categoryName = xml.attribute(QStringLiteral("name"));

            DomElementIterator it(xml, QString());
            while (it.hasNext()) {
                QDomElement xml = it.next();

                if (xml.tagName() == QStringLiteral("AppLink"))
                    readAppLink(xml, categoryName);
            }
        }
    }

    QStringList pinnedLaunchers = m_settings->value("pinnedLaunchers").toStringList();
    for (const QString &appId : pinnedLaunchers)
        getApplication(appId)->setPinned(true);

    Q_EMIT refreshed();
}

void ApplicationManager::readAppLink(const QDomElement &xml, const QString &categoryName)
{
    QString desktopFile = xml.attribute(QStringLiteral("desktopFile"));
    QString appId = QFileInfo(desktopFile).completeBaseName();

    Application *app = getApplication(appId);

    if (!app->hasCategory(categoryName))
        app->m_categories.append(categoryName);
}
