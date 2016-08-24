#pragma once

#include <GreenIsland/Server/ApplicationManager>
#include <Hawaii/Settings/QGSettings>
#include <QtCore/QLoggingCategory>
#include <QtCore/QObject>

class QDomElement;
class Application;

using namespace Hawaii;

Q_DECLARE_LOGGING_CATEGORY(APPLICATION_MANAGER)

class ApplicationManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(GreenIsland::Server::ApplicationManager *applicationManager READ applicationManager
                       WRITE setApplicationManager NOTIFY applicationManagerChanged)

public:
    ApplicationManager(QObject *parent = nullptr);

    GreenIsland::Server::ApplicationManager *applicationManager() const;

    Application *getApplication(const QString &appId);

    QList<Application *> applications() const;
    QList<Application *> pinnedApps() const;

public Q_SLOTS:
    void quit(const QString &appId);
    void launch(const QString &appId);

    void setApplicationManager(GreenIsland::Server::ApplicationManager *appMan);

Q_SIGNALS:
    void applicationManagerChanged();
    void applicationLaunched(Application *app);
    void refreshed();
    void applicationAdded(Application *app);

private Q_SLOTS:
    void refresh();
    void handleApplicationAdded(QString appId, pid_t pid);
    void handleApplicationRemoved(QString appId, pid_t pid);
    void handleApplicationFocused(QString appId);

private:
    GreenIsland::Server::ApplicationManager *m_appMan = nullptr;
    QGSettings *m_settings = nullptr;
    QMap<QString, Application *> m_apps;

    void readAppLink(const QDomElement &xml, const QString &categoryName);
};
