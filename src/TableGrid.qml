import QtQuick
import QtQuick.Controls
import QtQml.Models

Rectangle {
    id: panel
    objectName: "markdownTableGrid"
    required property var modelData
    required property var preview
    required property color foreground
    required property color backgroundColor
    required property color borderColor
    required property font editorFont
    signal editorChanged(var editor)
    signal sourceRequested()
    property var activeEditor: null
    property string error: ""
    property bool restored: false
    property int currentColumn: 0
    property int currentRow: 0
    property var editingWidths: ({})
    function expandColumn(column, text) {
        var width = Math.min(4096, Math.ceil(metrics.advanceWidth(text)) + 40);
        if (width <= (editingWidths[column] || tableModel.columnWidth(column))) return;
        var widths = editingWidths; widths[column] = width; editingWidths = widths;
        grid.forceLayout();
    }
    readonly property var tableModel: modelData.model
    x: modelData.x
    y: modelData.y
    width: modelData.width
    height: modelData.height
    color: backgroundColor
    radius: 4
    border.color: borderColor
    clip: true
    function discardCell() {
        if (activeEditor) activeEditor.discarded = true;
        grid.closeEditor();
    }
    function commitCell() {
        if (!activeEditor) {
            if (Object.keys(tableModel.drafts).length) { error = "Correct or discard pending cell edits"; return false; }
            return true;
        }
        if (!activeEditor.commit()) return false;
        grid.closeEditor();
        if (Object.keys(tableModel.drafts).length) { error = "Correct or discard pending cell edits"; return false; }
        return true;
    }
    function restorePosition() {
        if (restored || grid.contentWidth <= 0) return;
        grid.contentX = Math.min(tableModel.scrollX, Math.max(0, grid.contentWidth - grid.width));
        grid.contentY = Math.min(tableModel.scrollY, Math.max(0, grid.contentHeight - grid.height));
        restored = true;
    }
    Row {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 3
        height: 26
        spacing: 4
        Label { text: panel.error || "Table"; color: panel.foreground; height: 26; verticalAlignment: Text.AlignVCenter; font.pixelSize: 12 }
        ToolButton {
            objectName: "tableSourceButton"
            text: "Source"
            focusPolicy: Qt.NoFocus
            height: 26
            onClicked: if (panel.commitCell()) { panel.preview.setTableSource(panel.modelData.id, true); panel.sourceRequested(); }
        }
        ToolButton {
            text: "⋯"
            focusPolicy: Qt.NoFocus
            height: 26
            onClicked: actions.popup()
            FontMetrics { id: menuMetrics; font: actions.font }
            Menu {
                id: actions
                objectName: "tableActionsMenu"
                width: Math.max(320, menuMetrics.advanceWidth("Discard pending cell edits") + 80)
                MenuItem { text: "Insert row below"; onTriggered: if (panel.commitCell()) Qt.callLater(function() { panel.tableModel.insertRow(Math.max(0, panel.currentRow)); }); }
                MenuItem { text: "Delete row"; enabled: panel.currentRow > 0; onTriggered: if (panel.commitCell()) Qt.callLater(function() { panel.tableModel.removeRow(panel.currentRow); }); }
                MenuItem {
                    text: "Copy pending cell drafts"
                    enabled: Object.keys(panel.tableModel.drafts).length > 0
                    onTriggered: panel.preview.copyTableDrafts(panel.modelData.id)
                }
                MenuItem {
                    text: "Discard pending cell edits"
                    enabled: Object.keys(panel.tableModel.drafts).length > 0
                    onTriggered: {
                        if (panel.activeEditor) panel.activeEditor.discarded = true;
                        grid.closeEditor(); panel.tableModel.drafts = {}; panel.error = "";
                    }
                }
                MenuSeparator {}
                MenuItem { objectName: "alignColumnLeft"; text: "Align column left"; onTriggered: if (panel.commitCell()) panel.tableModel.setColumnAlignment(panel.currentColumn, "left"); }
                MenuItem { objectName: "alignColumnCenter"; text: "Align column center"; onTriggered: if (panel.commitCell()) panel.tableModel.setColumnAlignment(panel.currentColumn, "center"); }
                MenuItem { objectName: "alignColumnRight"; text: "Align column right"; onTriggered: if (panel.commitCell()) panel.tableModel.setColumnAlignment(panel.currentColumn, "right"); }
                MenuSeparator {}
                MenuItem { text: "Insert column to the right"; onTriggered: if (panel.commitCell()) Qt.callLater(function() { panel.tableModel.insertColumn(Math.max(0, panel.currentColumn)); }); }
                MenuItem { text: "Delete column"; enabled: grid.columns > 1; onTriggered: if (panel.commitCell()) Qt.callLater(function() { panel.tableModel.removeColumn(panel.currentColumn); }); }
            }
        }
    }
    TableView {
        id: grid
        objectName: "tableCellView"
        x: 1
        y: 32
        width: parent.width - 2
        height: parent.height - y - 14
        clip: true
        model: panel.tableModel
        reuseItems: true
        boundsBehavior: Flickable.StopAtBounds
        editTriggers: TableView.SingleTapped | TableView.EditKeyPressed
        selectionModel: ItemSelectionModel { model: grid.model }
        columnWidthProvider: function(column) { return Math.max(panel.tableModel.columnWidth(column), panel.editingWidths[column] || 0); }
        rowHeightProvider: function(row) { return Math.ceil(metrics.height) + 16; }
        FontMetrics { id: metrics; font: panel.editorFont }
        onContentWidthChanged: Qt.callLater(panel.restorePosition)
        onContentXChanged: if (panel.restored) panel.tableModel.scrollX = contentX
        onContentYChanged: if (panel.restored) panel.tableModel.scrollY = contentY
        Component.onCompleted: Qt.callLater(panel.restorePosition)
        Connections {
            target: panel.tableModel
            function onDataChanged() { grid.forceLayout(); }
        }
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; focusPolicy: Qt.NoFocus }
        delegate: Rectangle {
            id: cell
            objectName: "tableCellDelegate"
            required property int row
            required property int column
            required property string markdown
            required property string html
            required property bool header
            required property bool current
            required property bool editing
            required property int alignment
            implicitWidth: 100
            implicitHeight: Math.ceil(metrics.height) + 16
            color: current ? Qt.rgba(panel.foreground.r, panel.foreground.g, panel.foreground.b, 0.08) : panel.backgroundColor
            border.width: 0.5
            border.color: panel.borderColor
            clip: true
            Text {
                objectName: "tableCellDisplay"
                visible: !cell.editing
                horizontalAlignment: cell.alignment
                anchors.fill: parent
                anchors.margins: 8
                text: panel.tableModel.drafts[cell.row + ":" + cell.column] ? panel.tableModel.drafts[cell.row + ":" + cell.column].text : cell.html
                textFormat: panel.tableModel.drafts[cell.row + ":" + cell.column] ? Text.PlainText : Text.RichText
                wrapMode: Text.NoWrap
                font.family: panel.editorFont.family
                font.pixelSize: panel.editorFont.pixelSize
                font.bold: cell.header
                color: panel.foreground
            }
            TableView.editDelegate: TextField {
                id: field
                objectName: "tableCellEditor"
                anchors.fill: parent
                font: panel.editorFont
                text: cell.markdown
                selectByMouse: true
                horizontalAlignment: cell.alignment
                background: Rectangle { color: panel.backgroundColor; border.color: field.activeFocus ? field.palette.highlight : panel.borderColor; border.width: 1 }
                function revealCursor() {
                    if (!activeFocus) return;
                    var p = mapToItem(grid, cursorRectangle.x, 0);
                    var x = grid.contentX;
                    if (p.x < 8) x += p.x - 8;
                    else if (p.x + cursorRectangle.width > grid.width - 8) x += p.x + cursorRectangle.width - grid.width + 8;
                    grid.contentX = Math.max(0, Math.min(x, grid.contentWidth - grid.width));
                }
                onCursorRectangleChanged: Qt.callLater(revealCursor)
                onPreeditTextChanged: { panel.expandColumn(cell.column, text + preeditText); Qt.callLater(revealCursor); }
                property string original: ""
                property var revision: 0
                function storeDraft() {
                    var drafts = panel.tableModel.drafts;
                    var key = (revision === panel.tableModel.draftRevision ? "" : "recovered/" + revision + "/") + cell.row + ":" + cell.column;
                    drafts[key] = { text: text, original: original };
                    panel.tableModel.drafts = drafts;
                }
                function clearDraft() {
                    var drafts = panel.tableModel.drafts; delete drafts[cell.row + ":" + cell.column]; panel.tableModel.drafts = drafts;
                }
                property bool discarded: false
                property bool committed: false
                function commit() {
                    if (discarded || committed) return true;
                    if (inputMethodComposing) Qt.inputMethod.commit();
                    if (inputMethodComposing) return false;
                    if (revision !== panel.tableModel.draftRevision || cell.markdown !== original || !panel.tableModel.setCell(cell.row, cell.column, text)) {
                        storeDraft();
                        panel.error = "Cell not saved: correct or discard its draft";
                        return false;
                    }
                    clearDraft();
                    committed = true;
                    panel.error = "";
                    return true;
                }
                onTextEdited: { committed = false; storeDraft(); panel.expandColumn(cell.column, text); Qt.callLater(revealCursor); }
                onEditingFinished: if (!discarded) commit()
                TableView.onCommit: commit()
                Keys.priority: Keys.BeforeItem
                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_Escape && !inputMethodComposing) {
                        discarded = true; clearDraft(); grid.closeEditor(); event.accepted = true;
                    } else if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && !inputMethodComposing) {
                        if (commit()) grid.closeEditor(); event.accepted = true;
                    } else if ((event.key === Qt.Key_Tab || event.key === Qt.Key_Backtab) && !inputMethodComposing && !commit()) event.accepted = true;
                }
                Component.onCompleted: {
                    var draft = panel.tableModel.drafts[cell.row + ":" + cell.column];
                    revision = panel.tableModel.draftRevision;
                    original = draft ? draft.original : cell.markdown;
                    if (draft) text = draft.text;
                    panel.currentColumn = cell.column; panel.currentRow = cell.row;
                    panel.activeEditor = field;
                    panel.editorChanged(field);
                    selectAll();
                }
                Component.onDestruction: {
                    if (panel.activeEditor === field) { panel.activeEditor = null; panel.editorChanged(null); }
                }
            }
        }
    }
    BlockOverflow {
        anchors.fill: parent
        contentWidth: grid.contentWidth
        viewportWidth: grid.width
        offset: grid.contentX
        onScrollRequested: function(offset) { grid.contentX = offset; }
    }
}
