/****************************************************************************
 * This file is part of Hawaii.
 *
 * Copyright (C) 2014-2016 Pier Luigi Fiorini
 * Copyright (C) 2014 Aaron Seigo <aseigo@kde.org>
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

import QtQuick 2.0
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.0
import Fluid.Controls 1.0
import Hawaii.Desktop 1.0
import Hawaii.Notifications 1.0
import "notifications" as NotificationsIndicator

Indicator {
    property int notificationId: 0
    property var pendingRemovals: []

    readonly property bool hasNotifications: notificationsView.count > 0
    readonly property bool silentMode: false

    title: qsTr("Notifications")
    iconName: silentMode ? "social/notifications_off"
                         : hasNotifications ? "social/notifications"
                                            : "social/notifications_none"
    component: ColumnLayout {
        spacing: 0

        Placeholder {
            Layout.fillWidth: true
            Layout.fillHeight: true

            iconName: "social/notifications_none"
            text: qsTr("No notifications")
            visible: notificationView.count == 0
        }

        ListView {
            id: notificationView
            spacing: Units.largeSpacing
            clip: true
            model: notificationsModel
            visible: count > 0
            delegate: NotificationsIndicator.NotificationListItem {}
            add: Transition {
                NumberAnimation {
                    properties: "x"
                    from: notificationView.width
                    duration: Units.shortDuration
                }
            }
            remove: Transition {
                NumberAnimation {
                    properties: "x"
                    to: notificationView.width
                    duration: Units.longDuration
                }

                NumberAnimation {
                    properties: "opacity"
                    to: 0
                    duration: Units.longDuration
                }
            }
            removeDisplaced: Transition {
                SequentialAnimation {
                    PauseAnimation {
                        duration: Units.longDuration
                    }

                    NumberAnimation {
                        properties: "x,y"
                        duration: Units.shortDuration
                    }
                }
            }

            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
    onClicked: badgeCount = 0

    Timer {
        id: pendingTimer
        interval: 5000
        repeat: false
        onTriggered: {
            var i;
            for (i = 0; i < pendingRemovals.length; ++i) {
                var id = pendingRemovals[i];
                var j;
                for (j = 0; j < notificationsModel.count; ++j) {
                    if (notificationsModel.get(j) == id)
                        notificationsModel.remove(j);
                }
            }

            pendingRemovals = [];
        }
    }

    Connections {
        target: NotificationsService
        onNotificationReceived: addNotification({id: notificationId, appName: appName,
                                                appIcon: appIcon, hasIcon: hasIcon,
                                                summary: summary, body: body,
                                                actions: actions, isPersistent: isPersistent,
                                                expireTimeout: expireTimeout, hints: hints})
        onNotificationClosed: repositionNotifications()
    }

    ListModel {
        id: notificationsModel
    }

    Component.onCompleted: {
        // Move notifications every time the available geometry changes
        //_greenisland_output.availableGeometryChanged.connect(repositionNotifications);
    }

    /*
        Notification data object has the following properties:
         - id
         - appName
         - appIcon
         - image
         - summary
         - body
         - actions
         - isPersistent
         - expireTimeout
         - hints
    */
    function addNotification(data) {
        // Do not show duplicated notifications
        // Remove notifications that are sent again (odd, but true)
        var i;
        for (i = 0; i < notificationsModel.count; ++i) {
            var tmp = notificationsModel.get(i);
            var matches = (tmp.appName === data.appName &&
                           tmp.summary === data.summary &&
                           tmp.body === data.body);
            var sameSource = tmp.id === data["id"];

            if (sameSource && matches)
                return;

            if (sameSource || matches) {
                notificationsModel.remove(i)
                break;
            }
        }

        if (!sameSource && !matches)
            badgeCount++;

        if (data["summary"].length < 1) {
            data["summary"] = data["body"];
            data["body"] = "";
        }

        notificationsModel.insert(0, data);
        if (!data["isPersistent"]) {
            pendingRemovals.push(data["id"]);
            pendingTimer.start();
        }

        // Print notification data
        console.debug(JSON.stringify(data));

        // Update notification window if it's the same source
        // otherwise create a new window
        if (sameSource) {
            var i, found = false;
            var popups = screenView.layers.notifications.children;
            for (i = 0; i < popups.length; i++) {
                var popup = popups[i];
                if (popup.objectName !== "notificationWindow")
                    continue;

                if (popup.notificationData.id === data["id"]) {
                    popup.populateNotification(data);
                    found = true;
                    break;
                }
            }

            if (!found)
                createNotificationWindow(data);
        } else {
            createNotificationWindow(data);
        }
    }

    function createNotificationWindow(data) {
        var component = Qt.createComponent("events/NotificationWindow.qml");
        if (component.status !== Component.Ready) {
            console.error(component.errorString());
            return;
        }

        var window = component.createObject(screenView.layers.notifications);
        window.heightChanged.connect(repositionNotifications);
        window.actionInvoked.connect(function(actionId) {
            NotificationsService.invokeAction(data["id"], actionId);
        });
        window.closed.connect(function(notificationWindow) {
            NotificationsService.closeNotification(data["id"], NotificationsService.CloseReasonByUser);
            notificationWindow.close();
            repositionNotifications();
        });
        window.expired.connect(function(notificationWindow) {
            NotificationsService.closeNotification(data["id"], NotificationsService.CloseReasonExpired);
            notificationWindow.close();
            repositionNotifications();
        });
        window.populateNotification(data);
        repositionNotifications();
        window.show();
    }

    function repositionNotifications() {
        var popups = screenView.layers.notifications.children;
        var workArea = output.availableGeometry;
        var offset = Units.gridUnit;
        var totalHeight = 0;

        var i;
        for (i = 0; i < popups.length; i++) {
            var popup = popups[i];
            if (popup.objectName !== "notificationWindow")
                continue;

            // Ignore notification windows that are about to be destroyed
            if (popup.closing)
                continue;

            popup.x = workArea.width - popup.width - offset;
            popup.y = workArea.height - totalHeight - popup.height;
            totalHeight += popup.height + offset;
        }
    }
}
