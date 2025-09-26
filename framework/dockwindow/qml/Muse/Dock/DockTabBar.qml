/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore
 * Music Composition & Notation
 *
 * Copyright (C) 2023 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
import QtQuick 2.15

import Muse.Ui 1.0
import Muse.UiComponents 1.0
import Muse.Dock 1.0

Rectangle {
    id: root

    clip: true

    color: ui.theme.backgroundSecondaryColor

    property QtObject tabBarCpp: null

    property alias tabsModel: tabs.model
    property alias currentIndex: tabs.currentIndex

    property bool draggingTabsAllowed: true

    property NavigationPanel navigationPanel: null
    readonly property string currentItemNavigationName: tabs.currentItem && tabs.currentItem.navigation ? tabs.currentItem.navigation.name : ""

    signal tabClicked(int index)
    signal handleContextMenuItemRequested(string itemId)

    function updateMouseArea() {
        if (tabBarCpp) {
            tabBarCpp.setDraggableMouseArea(draggingTabsMouseArea)
            tabBarCpp.tabBarQmlItem = this
        }
    }

    function updateToolBarComponent() {
        const idx = root.tabsModel.index(root.currentIndex, 0);
        const toolBar = root.tabsModel.data(idx, DockTabsModel.ToolBarComponent)
        toolbarLoader.sourceComponent = toolBar
    }

    function doubleClicked(pos) {
        if (root.tabBarCpp) {
            root.tabBarCpp.doubleClicked(pos)
        }
    }

    Component.onDestruction: { root.tabsModel.clear() }

    onTabBarCppChanged: { updateMouseArea() }
    onDraggingTabsAllowedChanged: { updateMouseArea() }
    onCurrentIndexChanged: { updateToolBarComponent() }

    Connections {
        target: root.tabsModel
        function onDataChanged() { updateToolBarComponent() }
        function onModelReset() { updateToolBarComponent() }
    }

    ListView {
        id: tabs

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left

        width: Math.min(contentWidth, availableWidth)

        orientation: Qt.Horizontal
        interactive: false
        spacing: 0

        readonly property real availableWidth: root.width + 1  // + 1, because we don't need to see the rightmost separator
        readonly property real implicitWidthOfActiveTab: currentItem ? currentItem.implicitWidth : 0
        readonly property real implicitWidthOfAllTabsTogether: {
            let result = 0
            let items = tabs.contentItem.children

            for (let i in items) {
                let item = items[i]
                if (item && item.implicitWidth) {
                    result += item.implicitWidth
                }
            }

            return result
        }

        delegate: DockPanelTab {
            text: model.title
            isCurrent: root.currentIndex === model.index
            contextMenuModel: model.contextMenu

            width: {
                var w
                if (isCurrent || tabs.implicitWidthOfAllTabsTogether <= tabs.availableWidth) {
                    w = implicitWidth
                } else {
                    w = (tabs.availableWidth - tabs.implicitWidthOfActiveTab)
                    / (tabs.implicitWidthOfAllTabsTogether - tabs.implicitWidthOfActiveTab)
                    * implicitWidth
                }
                return w
            }

            navigation.name: text
            navigation.panel: root.navigationPanel
            navigation.order: model.index * 2 // NOTE '...' button will have +1 order

            onNavigationTriggered: {
                root.tabClicked(model.index)
            }

            onClicked: {
                root.tabClicked(model.index)
            }

            onHandleContextMenuItemRequested: function(itemId) {
                root.handleContextMenuItemRequested(itemId)
            }
        }
    }

    MouseArea {
        id: draggingTabsMouseArea

        anchors.top: parent.top
        anchors.left: parent.left
        width: tabs.contentWidth
        height: root.height - bottomSeparator.height

        hoverEnabled: false
        propagateComposedEvents: true
        enabled: root.visible && root.draggingTabsAllowed
        cursorShape: Qt.SizeAllCursor

        onClicked: function(mouse) {
            if (!root.tabBarCpp) {
                return
            }
            // Hack - if the current tab changed on this click, do not propagate the click event
            // to the tab's menu button. We don't want the menu to appear when changing tabs.
            mouse.accepted = root.tabBarCpp.tabChangedOnClick
        }
    }

    Loader {
        id: toolbarLoader

        anchors.top: parent.top
        anchors.left: tabs.right
        anchors.right: parent.right
        anchors.bottom: bottomSeparatorContainer.top
    }

    Item {
        id: bottomSeparatorContainer

        anchors.left: tabs.right
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        height: bottomSeparator.height

        SeparatorLine { id: bottomSeparator }
    }
}
