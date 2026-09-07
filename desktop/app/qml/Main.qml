import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Todolist 1.0

ApplicationWindow {
    id: root
    width: 420
    height: 640
    visible: true
    title: qsTr("ToDoList")

    // 由 main.cpp 打开本地库后注入
    TodoListModel {
        id: todos
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: input
                Layout.fillWidth: true
                placeholderText: qsTr("新待办…")
                onAccepted: add()
                Keys.onReturnPressed: add()
            }
            Button {
                text: qsTr("添加")
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
                height: 44
                color: index % 2 === 0 ? "#f7f7f7" : "#ffffff"
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    CheckBox {
                        checked: done
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
                        onClicked: todos.removeItem(index)
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: todos.ready
                  ? (todos.rowCount() === 0 ? qsTr("（暂无待办，输入后回车添加）")
                                            : qsTr("共 %1 项").arg(todos.rowCount()))
                  : qsTr("本地库未就绪")
            horizontalAlignment: Text.AlignHCenter
            color: "#888888"
        }
    }
}