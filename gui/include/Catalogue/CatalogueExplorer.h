#ifndef SCIQLOP_CATALOGUEEXPLORER_H
#define SCIQLOP_CATALOGUEEXPLORER_H

#include <Common/spimpl.h>
#include <QDialog>

namespace Ui {
class CatalogueExplorer;
}

class CatalogueEventsWidget;
class CatalogueSideBarWidget;

class VisualizationWidget;
class VisualizationSelectionZoneItem;

class DBEvent;


class CatalogueExplorer : public QDialog {
    Q_OBJECT

public:
    explicit CatalogueExplorer(QWidget *parent = 0);
    virtual ~CatalogueExplorer();

    void setVisualizationWidget(VisualizationWidget *visualization);

    CatalogueEventsWidget &eventsWidget() const;
    CatalogueSideBarWidget &sideBarWidget() const;

    void clearSelectionZones();
    void addSelectionZoneItem(const std::shared_ptr<DBEvent> &event, const QString &productId,
                              VisualizationSelectionZoneItem *selectionZone);

private:
    Ui::CatalogueExplorer *ui;

    class CatalogueExplorerPrivate;
    spimpl::unique_impl_ptr<CatalogueExplorerPrivate> impl;
};

#endif // SCIQLOP_CATALOGUEEXPLORER_H