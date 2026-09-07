import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 480
    height: 680
    visible: true
    title: qsTr("ToDoList — 本地 + 云同步")

    // 模型与 SyncController 由 C++ 注入（todos / sync）

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        // --- 同步设置栏 ---
        GroupBox {
            title: qsTr("同步（服务器地址 → 注册设备 → 同步）")
            Layout.fillWidth: true
            ColumnLayout {
                anchors.fill: parent
                spacing: 6
                RowLayout {
                    Layout.fillWidth: true
                    TextField {
                        id: serverInput
                        Layout.fillWidth: true
                        text: sync.serverUrl
                        placeholderText: qsTr("http://host:port/api/v1")
                        enabled: !sync.syncing
                        onEditingFinished: sync.serverUrl = text.trim()
                    }
                    Button {
                        text: qsTr("注册设备")
                        enabled: !sync.syncing && serverInput.text.trim().length > 0
                        onClicked: {
                            sync.serverUrl = serverInput.text.trim()
                            sync.registerDevice()
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Button {
                        text: qsTr("立即同步")
                        enabled: !sync.syncing && sync.token.length > 0
                        onClicked: sync.startSync()
                    }
                    Text {
                        Layout.fillWidth: true
                        text: sync.statusText
                        elide: Text.ElideRight
                        color: sync.statusText.indexOf("失败") >= 0 ? "#c62828" : "#444444"
                    }
                    BusyIndicator {
                        running: sync.syncing
                        visible: sync.syncing
                        implicitWidth: 22
                        implicitHeight: 22
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: input
                Layout.fillWidth: true
                placeholderText: qsTr("新待办…（回车添加）")
                enabled: !sync.syncing
                onAccepted: add()
            }
            Button {
                text: qsTr("添加")
                enabled: !sync.syncing
                onClicked: add()
            }
            function add() {
                todos.addItem(input.text)
                input.clear()
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: todos
            clip: true
            delegate: Rectangle {
                width: list.width
                height: 46
                color: index % 2 === 0 ? "#f7f7f7" : "#ffffff"
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    CheckBox {
                        checked: done
                        enabled: !sync.syncing
                        onClicked: todos.setDone(index, checked)
                    }
                    Text {
                        Layout.fillWidth: true
                        text: content
                        elide: Text.ElideRight
                        color: done ? "#999999" : "#222222"
                        font.strikeout: done
                    }
                    Button {
                        text: qsTr("删除")
                        enabled: !sync.syncing
                        onClicked: todos.removeItem(index)
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: todos.ready
                  ? (list.count === 0
                     ? qsTr("（暂无待办，输入后回车添加；编辑后点「立即同步」上传）")
                     : qsTr("共 %1 项 · 本地库: %2").arg(list.count).arg(todos.ready ? "就绪" : ""))
                  : qsTr("本地库未就绪")
            horizontalAlignment: Text.AlignHCenter
            color: "#888888"
        }
    }
}