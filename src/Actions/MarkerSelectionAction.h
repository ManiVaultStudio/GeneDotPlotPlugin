#pragma once

#include <actions/GroupAction.h>
#include <actions/StringAction.h>
#include <actions/TriggerAction.h>

#include <QPointer>
#include <QListWidget>

/** Marker search, ordered gene selection and group visibility controls. */
class MarkerSelectionAction : public mv::gui::GroupAction
{
    Q_OBJECT

public:
    MarkerSelectionAction(QObject* parent, const QString& title);

    QListWidget* getGeneSelectionListWidget() const { return _geneSelectionListWidget; }
    QListWidget* getGroupSelectionListWidget() const { return _groupSelectionListWidget; }
    mv::gui::StringAction& getGeneSearchStringAction() { return _geneSearchStringAction; }
    const mv::gui::StringAction& getGeneSearchStringAction() const { return _geneSearchStringAction; }

    void queueSelectionState(const QVariantList& geneSelectionState, const QVariantList& groupSelectionState, const QString& geneSearchString);
    void applyPendingSelectionState();
    void fromVariantMap(const QVariantMap& variantMap) override;
    QVariantMap toVariantMap() const override;

signals:
    void selectionChanged();

protected:
    QWidget* getWidget(QWidget* parent, const std::int32_t& widgetFlags) override;

private:
    void filterGenes(const QString& searchString);
    void selectMarkerGenes();
    void setAllGroupsVisible(bool visible);

    mv::gui::StringAction _geneSearchStringAction;
    mv::gui::TriggerAction _pasteMarkerGenesTriggerAction;
    mv::gui::TriggerAction _showAllGroupsTriggerAction;
    mv::gui::TriggerAction _hideAllGroupsTriggerAction;
    QPointer<QListWidget> _geneSelectionListWidget;
    QPointer<QListWidget> _groupSelectionListWidget;
    QVariantList _pendingGeneSelectionState;
    QVariantList _pendingGroupSelectionState;
    QString _pendingGeneSearchString;
    bool _hasPendingSelectionState = false;
};
