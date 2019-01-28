#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "editprofile.h"
#include "ui_editprofile.h"
#include "newprofile.h"
#include "monitor.h"
#include "plotwidget.h"
#include "nvidia.h"
#include <NVCtrl/NVCtrl.h>


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    QWidget::setWindowIcon(QIcon("gpuonfire.svg"));

    checkForRoot();
    checkForProfiles();
    //queryGPUSettings();
    loadProfileSettings();
    /*queryDriverSettings();
    queryGPUs();*/
    tabHandler(ui->tabWidget->currentIndex());

    // Create persistent nvidia pointer
    nvidia *nvd = new nvidia;
    nv = nvd;
    nv->setupXNVCtrl();
    nv->setupNVML(0);
    nv->queryGPUFeatures();
    nv->queryGPUFreqOffset(0);
    nv->queryGPUMemClkOffset(0);
    nv->queryGPUVoltageOffset(0);
    nv->queryGPUPowerLimit(currentGPUIndex);
    nv->queryGPUPowerLimitAvailability(currentGPUIndex);
    nv->queryGPUPowerLimitLimits(currentGPUIndex);
    nv->queryGPUCurrentMaxClocks(currentGPUIndex);
    qDebug() << nv->GPUList.size() << nv->GPUList[0].name << "gpus from main";
    // Populate the GPU combo box
    for (int i=0; i<nv->gpuCount; i++) {
        ui->GPUComboBox->addItem("GPU-" + QString::number(i) + ": " + nv->GPUList[i].name);
    }

    setupMonitorTab();
    setupGraphMonitorTab();

    // Enable sliders according to GPU properties
    if (nv->GPUList[currentGPUIndex].overClockAvailable) {
        ui->frequencySlider->setRange(nv->GPUList[currentGPUIndex].minCoreClkOffset, nv->GPUList[currentGPUIndex].maxCoreClkOffset);
        ui->frequencySpinBox->setRange(nv->GPUList[currentGPUIndex].minCoreClkOffset, nv->GPUList[currentGPUIndex].maxCoreClkOffset);
        ui->frequencySlider->setValue(nv->GPUList[currentGPUIndex].coreClkOffset);
        ui->frequencySpinBox->setValue(nv->GPUList[currentGPUIndex].coreClkOffset);

        // Divide by 2 to get the clock speed
        ui->memClkSlider->setRange(nv->GPUList[currentGPUIndex].minMemClkOffset/2, nv->GPUList[currentGPUIndex].maxMemClkOffset/2);
        ui->memClkSpinBox->setRange(nv->GPUList[currentGPUIndex].minMemClkOffset/2, nv->GPUList[currentGPUIndex].maxMemClkOffset/2);
        ui->memClkSlider->setValue(nv->GPUList[currentGPUIndex].memClkOffset/2);
        ui->memClkSpinBox->setValue(nv->GPUList[currentGPUIndex].memClkOffset/2);
    } else {
        ui->frequencySlider->setEnabled(false);
        ui->frequencySpinBox->setEnabled(false);
        ui->memClkSlider->setEnabled(false);
        ui->memClkSpinBox->setEnabled(false);
    }

    if (nv->GPUList[currentGPUIndex].powerLimitAvailable) {
        ui->powerLimSlider->setRange(nv->GPUList[currentGPUIndex].minPowerLim/1000, nv->GPUList[currentGPUIndex].maxPowerLim/1000);
        ui->powerLimSpinBox->setRange(nv->GPUList[currentGPUIndex].minPowerLim/1000, nv->GPUList[currentGPUIndex].maxPowerLim/1000);
        ui->powerLimSlider->setValue(nv->GPUList[currentGPUIndex].powerLim/1000);
        ui->powerLimSpinBox->setValue(nv->GPUList[currentGPUIndex].powerLim/1000);
    } else {
        ui->powerLimSlider->setEnabled(false);
        ui->powerLimSpinBox->setEnabled(false);
    }

    if (nv->GPUList[currentGPUIndex].overVoltAvailable) {
        ui->voltageSlider->setRange(nv->GPUList[currentGPUIndex].minVoltageOffset/1000, nv->GPUList[currentGPUIndex].maxVoltageOffset/1000);
        ui->voltageSpinBox->setRange(nv->GPUList[currentGPUIndex].minVoltageOffset/1000, nv->GPUList[currentGPUIndex].maxVoltageOffset/1000);
        ui->voltageSlider->setValue(nv->GPUList[currentGPUIndex].voltageOffset/1000);
        ui->voltageSpinBox->setValue(nv->GPUList[currentGPUIndex].voltageOffset/1000);
    } else {
        ui->voltageSlider->setEnabled(false);
        ui->voltageSpinBox->setEnabled(false);
    }

    ui->fanSlider->setValue(nv->GPUList[currentGPUIndex].fanSpeed);
    ui->fanSpinBox->setValue(nv->GPUList[currentGPUIndex].fanSpeed);
    ui->fanSlider->setRange(0, 100);
    ui->fanSpinBox->setRange(0, 100);

    connect(fanUpdateTimer, SIGNAL(timeout()), this, SLOT(fanSpeedUpdater()));
    fanUpdateTimer->start(2000);

    connect(ui->frequencySpinBox, SIGNAL(valueChanged(int)), SLOT(resetTimer()));
    connect(ui->powerLimSpinBox, SIGNAL(valueChanged(int)), SLOT(resetTimer()));
    connect(ui->memClkSpinBox, SIGNAL(valueChanged(int)), SLOT(resetTimer()));
    connect(ui->voltageSpinBox, SIGNAL(valueChanged(int)), SLOT(resetTimer()));

    connect(ui->tabWidget, SIGNAL(currentChanged(int)), SLOT(tabHandler(int)));
    connect(monitorUpdater, SIGNAL(timeout()), SLOT(updateMonitor()));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_actionEdit_current_profile_triggered(bool)
{
    editProfile *editprof = new editProfile(this);
    editprof->show();
}

void MainWindow::checkForRoot()
{
    QProcess process;
    process.start("/bin/sh -c \"echo $EUID\"");
    process.waitForFinished();
    QString EUID = process.readLine();
    if (EUID.toInt() == 0) {
        isRoot = true;
        qDebug() << "Running as root";
    } else {
        qDebug() << "Running as normal user";
    }
}
void MainWindow::tabHandler(int index)
{
    // Disconnect the monitor updater when the tab is not visible
    // Maybe do this with ifs if the tabs can be moved
    switch (index) {
        case 2:
            monitorUpdater->start(1000);
            break;
        case 1:
            monitorUpdater->start(1000);
            break;
        default:
            monitorUpdater->stop();
            break;
    }
}
void MainWindow::setupMonitorTab()
{
    // Set the behavior of the tree view
    ui->monitorTree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->monitorTree->setIndentation(0);
    // Add the items
    gputemp->setText(0, "Core Temperature");
    powerdraw->setText(0, "Power Draw");
    voltage->setText(0, "Core Voltage");
    coreclock->setText(0, "Core Clock Frequency");
    memclock->setText(0, "Memory Clock Frequency");
    coreutil->setText(0, "Core Utilization");
    memutil->setText(0, "Memory Utilization");
    fanspeed->setText(0, "Fan Speed");
    memusage->setText(0, "Used Memory/Total Memory");
    curmaxclk->setText(0, "Maximum Core Clock Frequency");
    curmaxmemclk->setText(0, "Maximum Memory Clock Frequency");

    ui->monitorTree->addTopLevelItem(gputemp);
    ui->monitorTree->addTopLevelItem(powerdraw);
    ui->monitorTree->addTopLevelItem(voltage);
    ui->monitorTree->addTopLevelItem(coreclock);
    ui->monitorTree->addTopLevelItem(memclock);
    ui->monitorTree->addTopLevelItem(coreutil);
    ui->monitorTree->addTopLevelItem(memutil);
    ui->monitorTree->addTopLevelItem(fanspeed);
    ui->monitorTree->addTopLevelItem(memusage);
    ui->monitorTree->addTopLevelItem(curmaxclk);
    ui->monitorTree->addTopLevelItem(curmaxmemclk);
    // These values only change when the apply button has been pressed
    QString curMaxClk = QString::number(nv->GPUList[currentGPUIndex].maxCoreClk) + " MHz";
    curmaxclk->setText(1, curMaxClk);
    QString curMaxMemClk = QString::number(nv->GPUList[currentGPUIndex].maxMemClk) + " MHz";
    curmaxmemclk->setText(1, curMaxMemClk);
}
void MainWindow::setupGraphMonitorTab()
{
    monitor mon;
    mon.queryValues();
    fanSpeedUpdater();
    plotCmdsList.append(powerdrawplot);
    plotCmdsList.append(tempplot);
    plotCmdsList.append(coreclkplot);
    plotCmdsList.append(memclkplot);
    plotCmdsList.append(coreutilplot);
    plotCmdsList.append(memutilplot);
    plotCmdsList.append(voltageplot);
    plotCmdsList.append(fanspeedplot);
    // Layout for the plots
    plotWidget->setLayout(plotLayout);

    // Define the structs
    plotCmdsList[0].plot = tempPlot;
    plotCmdsList[0].vector = qv_temp;
    plotCmdsList[0].layout = tempLayout;
    //plotCmdsList[0].widget = tempWidget;

    plotCmdsList[1].plot = powerDrawPlot;
    plotCmdsList[1].vector = qv_powerDraw;
    plotCmdsList[1].layout = powerDrawLayout;
    //plotCmdsList[1].widget = powerDrawWidget;

    plotCmdsList[2].plot = coreClkPlot;
    plotCmdsList[2].vector = qv_coreClk;
    plotCmdsList[2].layout = coreClkLayout;
    //plotCmdsList[2].widget = coreClkWidget;

    plotCmdsList[3].plot = memClkPlot;
    plotCmdsList[3].vector = qv_memClk;
    plotCmdsList[3].layout = memClkLayout;
    //plotCmdsList[3].widget = memClkWidget;

    plotCmdsList[4].plot = coreUtilPlot;
    plotCmdsList[4].vector = qv_coreUtil;
    plotCmdsList[4].layout = coreUtilLayout;
    //plotCmdsList[4].widget = coreUtilWidget;

    plotCmdsList[5].plot = memUtilPlot;
    plotCmdsList[5].vector = qv_memUtil;
    plotCmdsList[5].layout = memUtilLayout;
    //plotCmdsList[5].widget = memUtilWidget;

    plotCmdsList[6].plot = voltagePlot;
    plotCmdsList[6].vector = qv_voltage;
    plotCmdsList[6].layout = voltageLayout;
    //plotCmdsList[6].widget = voltageWidget;

    plotCmdsList[7].plot = fanSpeedPlot;
    plotCmdsList[7].vector = qv_fanSpeed;
    plotCmdsList[7].layout = fanSpeedLayout;
    //plotCmdsList[7].widget = fanSpeedWidget;

    plotCmdsList[0].valueq = mon.temp.toDouble();
    plotCmdsList[1].valueq = mon.powerdraw.toDouble();
    plotCmdsList[2].valueq = mon.coreclock.toDouble();
    plotCmdsList[3].valueq = mon.memclock.toDouble();
    plotCmdsList[4].valueq = mon.coreutil.toDouble();
    plotCmdsList[5].valueq = mon.memutil.toDouble();
    plotCmdsList[6].valueq = mon.voltage.toDouble();
    plotCmdsList[7].valueq = fanSpeed;
    // Get the main widget palette and use it for the graphs
    QPalette palette;
    palette.setCurrentColorGroup(QPalette::Active);
    QColor color = palette.color(QPalette::Background);
    QColor textColor = palette.color(QPalette::Text);
    QColor graphColor = palette.color(QPalette::Highlight);
    QPen graphPen;
    graphPen.setWidth(2);
    graphPen.setColor(graphColor);
    QPen tickPen;
    tickPen.setWidthF(0.5);
    tickPen.setColor(textColor);

    // Button to reset extreme values
    QPushButton *resetButton = new QPushButton(this);
    resetButton->setMaximumWidth(150);
    resetButton->setText("Reset min/max");
    connect(resetButton, SIGNAL(clicked()), SLOT(clearExtremeValues()));
    plotLayout->addWidget(resetButton);

    // Define features common to all plots

    for (int i=0; i<plotCmdsList.size(); i++) {
        plotCmdsList[i].plot->setMinimumHeight(220);
        plotCmdsList[i].plot->setMaximumHeight(220);
        plotCmdsList[i].plot->setMinimumWidth(200);

        plotCmdsList[i].plot->addGraph();
        plotCmdsList[i].plot->xAxis->setRange(-plotVectorSize +1, 0);
        plotCmdsList[i].plot->xAxis->setLabel("Time (s)");
        plotCmdsList[i].plot->installEventFilter(this);

        // Add the widget to the main layout
        QWidget *widget = new PlotWidget(this);
        plotCmdsList[i].widget = widget;
        connect(plotCmdsList[i].widget, SIGNAL(leftPlot()), SLOT(clearPlots()));

        plotCmdsList[i].widget->setLayout(plotCmdsList[i].layout);
        plotCmdsList[i].layout->addWidget(plotCmdsList[i].plot);
        plotLayout->addWidget(plotCmdsList[i].widget);

        plotCmdsList[i].plot->setBackground(color);
        plotCmdsList[i].plot->xAxis->setLabelColor(textColor);
        plotCmdsList[i].plot->yAxis->setLabelColor(textColor);
        plotCmdsList[i].plot->xAxis->setTickLabelColor(textColor);
        plotCmdsList[i].plot->yAxis->setTickLabelColor(textColor);
        plotCmdsList[i].plot->graph(0)->setPen(graphPen);
        plotCmdsList[i].plot->xAxis->setTickPen(tickPen);
        plotCmdsList[i].plot->yAxis->setTickPen(tickPen);
        plotCmdsList[i].plot->xAxis->setSubTickPen(tickPen);
        plotCmdsList[i].plot->yAxis->setSubTickPen(tickPen);
        plotCmdsList[i].plot->xAxis->setBasePen(tickPen);
        plotCmdsList[i].plot->yAxis->setBasePen(tickPen);

        QCPTextElement *minelem = new QCPTextElement(plotCmdsList[i].plot);
        plotCmdsList[i].mintext = minelem;
        minelem->setText("Min: " + QString::number(plotCmdsList[i].valueq));
        minelem->setTextColor(textColor);

        QCPTextElement *maxelem = new QCPTextElement(plotCmdsList[i].plot);
        plotCmdsList[i].maxtext = maxelem;
        maxelem->setText("Max: " + QString::number(plotCmdsList[i].valueq));
        maxelem->setTextColor(textColor);

        QCPTextElement *curelem = new QCPTextElement(plotCmdsList[i].plot);
        plotCmdsList[i].curtext = curelem;
        curelem->setText("Cur: " + QString::number(plotCmdsList[i].valueq));
        curelem->setTextColor(textColor);

        plotCmdsList[i].plot->plotLayout()->insertRow(0);
        QCPLayoutGrid *sublo = new QCPLayoutGrid;
        plotCmdsList[i].plot->plotLayout()->addElement(0, 0, sublo);
        sublo->setMargins(QMargins(5 ,5, 5, 5));
        sublo->addElement(plotCmdsList[i].mintext);
        sublo->addElement(plotCmdsList[i].maxtext);
        sublo->addElement(plotCmdsList[i].curtext);

        QCPItemText *text = new QCPItemText(plotCmdsList[i].plot);
        text->setColor(textColor);
        plotCmdsList[i].valText = text;

        // Set the y-range
        plotCmdsList[i].plot->yAxis->setRange(plotCmdsList[i].valueq -plotCmdsList[i].valueq*0.1, plotCmdsList[i].valueq + plotCmdsList[i].valueq*0.1);

        // Add the tracers
        QCPItemTracer *mouseTracer = new QCPItemTracer(plotCmdsList[i].plot);
        plotCmdsList[i].tracer = mouseTracer;
        mouseTracer->setStyle(QCPItemTracer::tsCrosshair);
        QPen tracerPen = graphPen;
        tracerPen.setWidthF(0.5);
        mouseTracer->setPen(tracerPen);
        connect(plotCmdsList[i].plot, SIGNAL(mouseMove(QMouseEvent*)), SLOT(plotHovered(QMouseEvent*)));

        plotCmdsList[i].maxval = plotCmdsList[i].valueq;
        plotCmdsList[i].minval = plotCmdsList[i].valueq;
    }

    tempPlot->yAxis->setLabel("Temperature (°C)");
    powerDrawPlot->yAxis->setLabel("Power Draw (W)");
    coreClkPlot->yAxis->setLabel("Core Clock Frequency (MHz)");
    memClkPlot->yAxis->setLabel("Memory Clock Frequency (MHz)");
    coreUtilPlot->yAxis->setLabel("Core Utilization (%)");
    memUtilPlot->yAxis->setLabel("Memory Utilization (%)");
    voltagePlot->yAxis->setLabel("Core Voltage (mV)");
    fanSpeedPlot->yAxis->setLabel("Fan Speed (%)");

    plotScrollArea->setWidget(plotWidget);
    plotScrollArea->setWidgetResizable(true);

    // Add scroll area to a layout so we can set it as the widget for the tab
    lo->addWidget(plotScrollArea);
    ui->monitorTab->setLayout(lo);

    //connect(plotHoverUpdater, SIGNAL(timeout()), SLOT(plotHovered()));
}
void MainWindow::updateMonitor()
{
    // Update the values for plots
    nv->queryGPUTemp(0);
    nv->queryGPUPowerDraw(0);
    nv->queryGPUFrequencies(0);
    nv->queryGPUUtils(0);
    nv->queryGPUVoltage(0);
    nv->queryGPUFanSpeed(0);
    nv->queryGPUUsedVRAM(0);

    // Remove the last decimal point from power draw to make it take less space on the plot
    double pwrdraw = nv->GPUList[currentGPUIndex].powerDraw;
    pwrdraw = pwrdraw/10;
    int num = static_cast<int>(pwrdraw);
    pwrdraw = static_cast<double>(num);
    pwrdraw = pwrdraw/100;

    plotCmdsList[0].valueq = nv->GPUList[0].temp;
    plotCmdsList[1].valueq = pwrdraw;
    plotCmdsList[2].valueq = nv->GPUList[0].coreFreq;
    plotCmdsList[3].valueq = nv->GPUList[0].memFreq;
    plotCmdsList[4].valueq = nv->GPUList[0].coreUtil;
    plotCmdsList[5].valueq = nv->GPUList[0].memUtil;
    plotCmdsList[6].valueq = nv->GPUList[0].voltage/1000;
    plotCmdsList[7].valueq = nv->GPUList[0].fanSpeed;

    qDebug() << monitorUpdater->remainingTime();

    gputemp->setText(1, QString::number(nv->GPUList[0].temp) + "°C");
    powerdraw->setText(1, QString::number(nv->GPUList[0].powerDraw/1000) + "W");
    voltage->setText(1, QString::number(nv->GPUList[0].voltage/1000) + "mV");
    coreclock->setText(1, QString::number(nv->GPUList[0].coreFreq) + "MHz");
    memclock->setText(1, QString::number(nv->GPUList[0].memFreq) + "MHz");
    coreutil->setText(1, QString::number(nv->GPUList[0].coreUtil) + "%");
    memutil->setText(1, QString::number(nv->GPUList[0].memUtil) + "%");
    fanspeed->setText(1, QString::number(nv->GPUList[0].fanSpeed) + "%");
    memusage->setText(1, QString::number(nv->GPUList[0].usedVRAM) + "/" + QString::number(nv->GPUList[0].totalVRAM) + "MB");

    // Decrement all time values by one
    for (int i=0; i<qv_time.length(); i++) {
        qv_time[i]--;
    }
    // Add current time (0)
    if (qv_time.size() < plotVectorSize) {
        qv_time.append(0);
    } else {
        qv_time.insert(plotVectorSize, 0);
    }
    // Remove the first elements if there are more elements than the x-range
    if (qv_time.size() > plotVectorSize) {
        qv_time.removeFirst();
    }

    for (int i=0; i<plotCmdsList.size(); i++) {
        // Check if the max/min values need to be updated
        if (!plotCmdsList[i].vector.isEmpty()) {
            plotCmdsList[i].curtext->setText("Cur: " + QString::number(plotCmdsList[i].valueq));
            if (plotCmdsList[i].maxval < plotCmdsList[i].valueq) {
                plotCmdsList[i].maxtext->setText("Max: " + QString::number(plotCmdsList[i].valueq));
                plotCmdsList[i].maxval = plotCmdsList[i].valueq;
            }
            if (plotCmdsList[i].minval > plotCmdsList[i].valueq) {
                plotCmdsList[i].mintext->setText("Min: " + QString::number(plotCmdsList[i].valueq));
                plotCmdsList[i].minval = plotCmdsList[i].valueq;
            }
        }
        if (plotCmdsList[i].vector.size() < plotVectorSize) {
            plotCmdsList[i].vector.append(plotCmdsList[i].valueq);
        } else {
            plotCmdsList[i].vector.insert(plotVectorSize, plotCmdsList[i].valueq);
        }
        // Remove the first element if there are more elements than the x-range
        if (plotCmdsList[i].vector.size() > plotVectorSize) {
            plotCmdsList[i].vector.removeFirst();
        }
        plotCmdsList[i].plot->graph(0)->setData(qv_time, plotCmdsList[i].vector);
        // If the newest value is out of bounds, resize the y-range
        if (plotCmdsList[i].valueq > plotCmdsList[i].plot->yAxis->range().upper) {
            plotCmdsList[i].plot->yAxis->setRangeUpper(plotCmdsList[i].valueq + plotCmdsList[i].valueq*0.1);
        }
        if (plotCmdsList[i].valueq < plotCmdsList[i].plot->yAxis->range().lower) {
            plotCmdsList[i].plot->yAxis->setRangeLower(plotCmdsList[i].valueq - plotCmdsList[i].valueq*0.1);
        }
        plotCmdsList[i].plot->replot();
        plotCmdsList[i].plot->update();
    }
    //qDebug() << monitorUpdater->remainingTime();
    // If the largest/smallest value is too far from the range end, this resizes them every 10th iteration of this function
    if (counter >= 10) {
        for (int i=0; i<plotCmdsList.size(); i++) {
            double lowestval = plotCmdsList[i].vector[0];
            double largestval = plotCmdsList[i].vector[0];
            for (int j=0; j<plotCmdsList[i].vector.size(); j++) {
                if (plotCmdsList[i].vector[j] < lowestval) {
                    lowestval = plotCmdsList[i].vector[j];
                }
                if (plotCmdsList[i].vector[j] > largestval) {
                    largestval = plotCmdsList[i].vector[j];
                }
            }
            if (plotCmdsList[i].plot->yAxis->range().upper - largestval*0.1 > largestval) {
                plotCmdsList[i].plot->yAxis->setRange(lowestval - lowestval*0.1, largestval + largestval*0.1);
            }
            if (plotCmdsList[i].plot->yAxis->range().lower + lowestval*0.1 < lowestval) {
                // Don't set the lower range to under 0
                if (lowestval - lowestval*0.1 < 0) {
                    plotCmdsList[i].plot->yAxis->setRangeLower(0);
                } else {
                    plotCmdsList[i].plot->yAxis->setRange(lowestval - lowestval*0.1, largestval + largestval*0.1);
                }
            }
        }
        counter = 0;
    }
    counter++;
}
void MainWindow::plotHovered(QMouseEvent *event)
{
    QPoint cursor = event->pos();

    int plotIndex = 0;
    for (int i=0; i<plotCmdsList.size(); i++) {
        if (plotCmdsList[i].widget->underMouse()) {
            plotIndex = i;
            break;
        }
    }
    double pointerxcoord = plotCmdsList[plotIndex].plot->xAxis->pixelToCoord(cursor.x());
    plotCmdsList[plotIndex].tracer->position->setCoords(pointerxcoord, plotCmdsList[plotIndex].plot->yAxis->range().upper);
    // Find the y-value for the corresponding coordinate
    int valIndex = 0;
    if (!qv_time.isEmpty() && pointerxcoord > -plotVectorSize*1.01 && pointerxcoord <= 0 + plotVectorSize*0.01) {
        double deltax = abs(qv_time[0] - pointerxcoord);
        for (int i=0; i<plotCmdsList[plotIndex].vector.size(); i++) {
            if (abs(qv_time[i] - pointerxcoord) < deltax) {
                deltax = abs(qv_time[i] - pointerxcoord);
                valIndex = i;
            }
        }
        plotCmdsList[plotIndex].valText->setText(QString::number(plotCmdsList[plotIndex].vector[valIndex]));
        // Make the text stay inside the plot
        if (pointerxcoord > -plotVectorSize*0.06) {
            plotCmdsList[plotIndex].valText->position->setCoords(-plotVectorSize*0.06, plotCmdsList[plotIndex].plot->yAxis->range().upper - plotCmdsList[plotIndex].plot->yAxis->range().size()*0.05);
        } else if (pointerxcoord < -plotVectorSize*0.94) {
            plotCmdsList[plotIndex].valText->position->setCoords(-plotVectorSize*0.94, plotCmdsList[plotIndex].plot->yAxis->range().upper - plotCmdsList[plotIndex].plot->yAxis->range().size()*0.05);
        } else {
            plotCmdsList[plotIndex].valText->position->setCoords(pointerxcoord, plotCmdsList[plotIndex].plot->yAxis->range().upper - plotCmdsList[plotIndex].plot->yAxis->range().size()*0.05);
        }
        QThread::msleep(10);
    } else {
        // If the cursor is not within the x-range, clear the text
        plotCmdsList[plotIndex].valText->setText("");
    }
    plotCmdsList[plotIndex].plot->update();
    plotCmdsList[plotIndex].plot->replot();
}
void MainWindow::clearPlots()
{
    for (int i=0; i<plotCmdsList.size(); i++) {
        plotCmdsList[i].valText->setText("");
        plotCmdsList[i].tracer->position->setCoords(1, plotCmdsList[i].plot->yAxis->range().upper);
        plotCmdsList[i].plot->replot();
        plotCmdsList[i].plot->update();
    }
}
void MainWindow::clearExtremeValues()
{
    for (int i=0; i<plotCmdsList.size(); i++) {
        plotCmdsList[i].minval = plotCmdsList[i].valueq;
        plotCmdsList[i].maxval = plotCmdsList[i].valueq;
        plotCmdsList[i].mintext->setText("Min: " + QString::number(plotCmdsList[i].valueq));
        plotCmdsList[i].maxtext->setText("Max: " + QString::number(plotCmdsList[i].valueq));
    }
}
void MainWindow::checkForProfiles()
{
    // If there are no profiles, create one, then list all the entries whose isProfile is true in the profile selection combo box
    QSettings settings("tuxclocker");
    QStringList groups = settings.childGroups();
    QString isProfile = "/isProfile";

    for (int i=0; i<groups.length(); i++) {
        // Make a query $profile/isProfile
        QString group = groups[i];
        QString query = group.append(isProfile);
        if (settings.value(query).toBool()) {
            noProfiles = false;
            break;
        }
    }
    if (noProfiles) {
        settings.setValue("New Profile/isProfile", true);
        settings.setValue("General/currentProfile", "New Profile");
        currentProfile = "New Profile";
    }
    // Redefine child groups so it contains the newly created profile if it was made
    QStringList newgroups = settings.childGroups();
    for (int i=0; i<newgroups.length(); i++) {
        // Make a query $profile/isProfile
        QString group = newgroups[i];
        QString query = group.append(isProfile);
        if (settings.value(query).toBool()) {
            ui->profileComboBox->addItem(newgroups[i]);
        }
    }
}

void MainWindow::on_profileComboBox_activated(const QString &arg1)
{
    // Change currentProfile to combobox selection
    if (currentProfile != arg1) {
        currentProfile = arg1;
        QSettings settings("tuxclocker");
        settings.setValue("General/currentProfile", currentProfile);
        loadProfileSettings();
    }
}

void MainWindow::getGPUDriver()
{
    QProcess process;
    process.start(queryForNvidiaProp);
    process.waitForFinished(-1);
    if (process.readAllStandardOutput().toInt() >= 1) {
        gpuDriver = "nvidia";
    }
}
void MainWindow::queryGPUs()
{
    QProcess process;
    process.start(nvGPUCountQ);
    process.waitForFinished();
    for (int i=0; i<process.readLine().toInt(); i++) {
        process.start(nvUUIDQ + " -i " + QString::number(i));
        process.waitForFinished();
        QString UUID = process.readLine();
        UUID.chop(1);
        UUIDList.append(UUID);
        process.start(queryGPUName + " -i " + QString::number(i));
        process.waitForFinished();
        QString GPUName = process.readLine();
        GPUName.chop(1);
        //ui->GPUComboBox->addItem("GPU-" + QString::number(i) + ": " + GPUName);
        //ui->GPUNameLabel->setText(GPUName);
    }
}
void MainWindow::fanSpeedUpdater()
{
    nv->queryGPUFanSpeed(currentGPUIndex);
    ui->fanSlider->setValue(nv->GPUList[currentGPUIndex].fanSpeed);
    ui->fanSpinBox->setValue(nv->GPUList[currentGPUIndex].fanSpeed);
}
void MainWindow::tempUpdater()
{
    QProcess process;
    process.start(nvTempQ);
    process.waitForFinished(-1);
    temp = process.readLine().toInt();
    if (xCurvePoints.size() != 0) {
        generateFanPoint();
    }
}
void MainWindow::resetTimer()
{
    // If a value has been changed this timer will start. When the apply button has been pressed, this gets cancelled
    connect(resettimer, SIGNAL(timeout()), SLOT(resetChanges()));
    resettimer->stop();
    resettimer->setSingleShot(true);
    resettimer->start(10000);
}
void MainWindow::resetChanges()
{
    // If the settings haven't been applied in 10 seconds, reset all values to their latest values
    ui->frequencySlider->setValue(nv->GPUList[currentGPUIndex].coreClkOffset);
    ui->frequencySpinBox->setValue(nv->GPUList[currentGPUIndex].coreClkOffset);

    ui->powerLimSlider->setValue(nv->GPUList[currentGPUIndex].powerLim/1000);
    ui->powerLimSpinBox->setValue(nv->GPUList[currentGPUIndex].powerLim/1000);

    ui->voltageSlider->setValue(nv->GPUList[currentGPUIndex].voltageOffset/1000);
    ui->voltageSpinBox->setValue(nv->GPUList[currentGPUIndex].voltageOffset/1000);

    ui->memClkSlider->setValue(nv->GPUList[currentGPUIndex].memClkOffset/2);
    ui->memClkSpinBox->setValue(nv->GPUList[currentGPUIndex].memClkOffset/2);
}
void MainWindow::queryDriverSettings()
{
    QProcess process;
    process.start(nvFanCtlStateQ);
    process.waitForFinished(-1);
    if (process.readLine().toInt() == 1) {
        manualFanCtl = true;
        ui->fanModeComboBox->setCurrentIndex(1);
    } else {
        manualFanCtl = false;
        ui->fanModeComboBox->setCurrentIndex(0);
    }
}
void MainWindow::applyFanMode()
{
    bool ret;
    qDebug() << "changing fanctl to" << nv->GPUList[currentGPUIndex].fanControlMode;
    switch (nv->GPUList[currentGPUIndex].fanControlMode) {
        case 0:
            // Driver controlled mode
            ret = nv->assignGPUFanCtlMode(currentGPUIndex, NV_CTRL_GPU_COOLER_MANUAL_CONTROL_FALSE);
            if (ret) {
                ui->fanSlider->setEnabled(false);
                ui->fanSpinBox->setEnabled(false);
            } else {
                ui->statusBar->showMessage("Failed to set fan mode", 5000);
            }
            break;
        case 1:
           // Static mode
            ret = nv->assignGPUFanCtlMode(currentGPUIndex, NV_CTRL_GPU_COOLER_MANUAL_CONTROL_TRUE);
            if (ret) {
                disconnect(fanUpdateTimer, SIGNAL(timeout()), this, SLOT(tempUpdater()));
                nv->assignGPUFanCtlMode(currentGPUIndex, ui->fanSlider->value());
                ui->fanSlider->setEnabled(true);
                ui->fanSpinBox->setEnabled(true);
            } else {
                ui->statusBar->showMessage("Failed to set fan mode", 5000);
            }
            break;
        case 2:
            // Custom mode
            ret = nv->assignGPUFanCtlMode(currentGPUIndex, NV_CTRL_GPU_COOLER_MANUAL_CONTROL_TRUE);
            if (ret) {
                connect(fanUpdateTimer, SIGNAL(timeout()), this, SLOT(tempUpdater()));
                ui->fanSlider->setEnabled(false);
                ui->fanSpinBox->setEnabled(false);
            } else {
                ui->statusBar->showMessage("Failed to set fan mode", 5000);
            }
            break;
    }
}
void MainWindow::queryGPUSettings()
{
    QProcess process;
    process.setReadChannelMode(QProcess::ForwardedErrorChannel);
    process.setReadChannel(QProcess::StandardOutput);
    process.start(nvVoltQ);
    process.waitForFinished(-1);
    voltInt = process.readLine().toInt()/1000;

    process.start(nvVoltOfsQ);
    process.waitForFinished(-1);
    voltOfsInt = process.readLine().toInt()/1000;
    latestVoltOfs = voltOfsInt;

    defVolt = voltInt - voltOfsInt;

    process.start(nvVoltOfsLimQ);
    process.waitForFinished(-1);
    for (int i=0; i<process.size(); i++) {
        if (process.readLine().toInt()/1000 > maxVoltOfsInt) {
            qDebug() << process.readLine();
            maxVoltOfsInt = process.readLine().toInt()/1000;
        }
    }

    process.start(nvCoreClkOfsQ);
    process.waitForFinished(-1);
    coreFreqOfsInt = process.readLine().toInt();
    latestClkOfs = coreFreqOfsInt;

    process.start(nvCurMaxClkQ);
    process.waitForFinished(-1);
    curMaxClkInt = process.readLine().toInt();

    process.start(nvMaxPowerLimQ);
    process.waitForFinished(-1);
    maxPowerLimInt = process.readLine().toInt();

    process.start(nvMinPowerLimQ);
    process.waitForFinished(-1);
    minPowerLimInt = process.readLine().toInt();

    process.start(nvCurPowerLimQ);
    process.waitForFinished(-1);
    curPowerLimInt = process.readLine().toInt();
    latestPowerLim = curPowerLimInt;

    process.start(nvClockLimQ);
    process.waitForFinished(-1);
    for (int i=0; i<process.size(); i++) {
        QString line = process.readLine();
        if (line.toInt() > maxCoreClkOfsInt) {
            maxCoreClkOfsInt = line.toInt();
        }
        if (line.toInt() <= minCoreClkOfsInt) {
            minCoreClkOfsInt = line.toInt();
        }
    }

    // This gets the transfer rate, the clock speed is rate/2
    process.start(nvMemClkLimQ);
    process.waitForFinished(-1);
    for (int i=0; i<process.size(); i++) {
        QString line = process.readLine();
        if (line.toInt()/2 > maxMemClkOfsInt) {
            maxMemClkOfsInt = line.toInt()/2;
        }
        if (line.toInt()/2 <= minMemClkOfsInt) {
            minMemClkOfsInt = line.toInt()/2;
        }
    }

    process.start(nvCurMaxMemClkQ);
    process.waitForFinished(-1);
    curMaxMemClkInt = process.readLine().toInt();

    process.start(nvCurMemClkOfsQ);
    process.waitForFinished(-1);
    memClkOfsInt = process.readLine().toInt()/2;
    latestMemClkOfs = memClkOfsInt;

    // Since the maximum core clock reported is the same on negative offsets as on 0 offset add a check here
    if (0 >= coreFreqOfsInt) {
        defCoreClk = curMaxClkInt;
    } else {
        defCoreClk = curMaxClkInt - coreFreqOfsInt;
    }
    defMemClk = curMaxMemClkInt - memClkOfsInt;

}
void MainWindow::applyGPUSettings()
{
    // Apply and save the values
    QSettings settings("tuxclocker");
    settings.beginGroup("General");
    settings.setValue("latestUUID", nv->GPUList[currentGPUIndex].uuid);
    settings.endGroup();
    settings.beginGroup(currentProfile);
    settings.beginGroup(nv->GPUList[currentGPUIndex].uuid);
    QProcess process;
    int offsetValue;
    int powerLimit;
    bool hadErrors = false;
    bool ret;
    errorText = "Failed to apply these settings: ";
    QString input = nvCoreClkSet;
    if (nv->GPUList[currentGPUIndex].coreClkOffset != ui->frequencySlider->value() && nv->GPUList[currentGPUIndex].overClockAvailable) {
        offsetValue = ui->frequencySlider->value();
        ret = nv->assignGPUFreqOffset(currentGPUIndex, offsetValue);
        if (ret) {
            nv->GPUList[currentGPUIndex].coreClkOffset = offsetValue;
            settings.setValue("clockFrequencyOffset", offsetValue);
        } else {
            errorText.append("- Core Clock Offset");
            ui->frequencySlider->setValue(nv->GPUList[currentGPUIndex].coreClkOffset);
            hadErrors = true;
        }
    }

    if (nv->GPUList[currentGPUIndex].memFreq/2 != ui->memClkSlider->value() && nv->GPUList[currentGPUIndex].overClockAvailable) {
        offsetValue = ui->memClkSlider->value();
        ret = nv->assignGPUMemClockOffset(currentGPUIndex, offsetValue*2);
        if (ret) {
            nv->GPUList[currentGPUIndex].memClkOffset = offsetValue*2;
            settings.setValue("memoryClockOffset", offsetValue*2);
        } else {
            errorText.append("- Memory Clock Offset");
            hadErrors = true;
            ui->frequencySlider->setValue(nv->GPUList[currentGPUIndex].memClkOffset/2);
        }
    }

    if (nv->GPUList[currentGPUIndex].powerLim/1000 != ui->powerLimSlider->value()) {
        powerLimit = ui->powerLimSlider->value();
        input = nvPowerLimSet;
        if (!isRoot) {
            // Need to ask for root somehow without running a specific program
            ret = nv->assignGPUPowerLimit(powerLimit*1000);
            if (ret) {
                nv->GPUList[currentGPUIndex].powerLim = powerLimit*1000;
                settings.setValue("powerLimit", powerLimit*1000);
            } else {
                errorText.append("- Power Limit");
                hadErrors = true;
                ui->powerLimSlider->setValue(nv->GPUList[currentGPUIndex].powerLim/1000);
            }
        } else {
            ret = nv->assignGPUPowerLimit(powerLimit*1000);
            if (ret) {
                nv->GPUList[currentGPUIndex].powerLim = powerLimit*1000;
                settings.setValue("powerLimit", powerLimit*1000);
            } else {
                errorText.append("- Power Limit");
                hadErrors = true;
                ui->powerLimSlider->setValue(nv->GPUList[currentGPUIndex].powerLim/1000);
            }
        }
    }

    if (latestVoltOfs != ui->voltageSlider->value() && nv->GPUList[currentGPUIndex].overVoltAvailable) {
        offsetValue = ui->voltageSlider->value();
        ret = nv->assignGPUVoltageOffset(currentGPUIndex, offsetValue*1000);
        if (ret) {
            nv->GPUList[currentGPUIndex].voltageOffset = offsetValue*1000;
                settings.setValue("voltageOffset", offsetValue*1000);
        } else {
            errorText.append("- Voltage Offset");
            hadErrors = true;
            ui->voltageSlider->setValue(nv->GPUList[currentGPUIndex].voltageOffset/1000);
        }
    }
    // Apply fan mode
    qDebug() << "changing fanctl to" << nv->GPUList[currentGPUIndex].fanControlMode;
    switch (nv->GPUList[currentGPUIndex].fanControlMode) {
        case 0:
            // Driver controlled mode
            ret = nv->assignGPUFanCtlMode(currentGPUIndex, NV_CTRL_GPU_COOLER_MANUAL_CONTROL_FALSE);
            if (ret) {
                ui->fanSlider->setEnabled(false);
                ui->fanSpinBox->setEnabled(false);
            } else {
                errorText.append("- Fan mode");
            }
            break;
        case 1:
           // Static mode
            ret = nv->assignGPUFanCtlMode(currentGPUIndex, NV_CTRL_GPU_COOLER_MANUAL_CONTROL_TRUE);
            if (ret) {
                disconnect(fanUpdateTimer, SIGNAL(timeout()), this, SLOT(tempUpdater()));
                nv->assignGPUFanCtlMode(currentGPUIndex, ui->fanSlider->value());
                ui->fanSlider->setEnabled(true);
                ui->fanSpinBox->setEnabled(true);
            } else {
                errorText.append("- Fan mode");
            }
            break;
        case 2:
            // Custom mode
            ret = nv->assignGPUFanCtlMode(currentGPUIndex, NV_CTRL_GPU_COOLER_MANUAL_CONTROL_TRUE);
            if (ret) {
                connect(fanUpdateTimer, SIGNAL(timeout()), this, SLOT(tempUpdater()));
                ui->fanSlider->setEnabled(false);
                ui->fanSpinBox->setEnabled(false);
            } else {
                errorText.append("- Fan mode");
            }
            break;
    }

    if (hadErrors) {
        ui->statusBar->showMessage(errorText, 5000);
    } else {
        ui->statusBar->showMessage("Settings applied", 5000);
    }
    resettimer->stop();
}

void MainWindow::loadProfileSettings()
{
    QSettings settings("tuxclocker");
    currentProfile = settings.value("General/currentProfile").toString();
    latestUUID = settings.value("General/latestUUID").toString();
    settings.beginGroup(currentProfile);
    settings.beginGroup(latestUUID);

    // Check for existance of the setting so zeroes don't get appended to curve point vectors
    if (settings.contains("xpoints")) {
        QString xPointStr = "/bin/sh -c \"echo " + settings.value("xpoints").toString() + grepStringToInt;
        QString yPointStr = "/bin/sh -c \"echo " + settings.value("ypoints").toString() + grepStringToInt;
        QProcess process;
        process.start(xPointStr);
        process.waitForFinished(-1);
        for (int i=0; i<process.size() +1; i++) {
            xCurvePoints.append(process.readLine().toInt());
        }
        process.start(yPointStr);
        process.waitForFinished(-1);
        for (int i=0; i<process.size() +1; i++) {
            yCurvePoints.append(process.readLine().toInt());
        }
        QStandardItemModel *model = qobject_cast<QStandardItemModel*>(ui->fanModeComboBox->model());
        QModelIndex customModeIndex = model->index(2, ui->fanModeComboBox->modelColumn());
        QStandardItem *customMode = model->itemFromIndex(customModeIndex);
        customMode->setEnabled(true);
        customMode->setToolTip("Use your own fan curve");
    } else {
        // Set fan mode "Custom" unselectable if there are no custom curve points
        QStandardItemModel *model = qobject_cast<QStandardItemModel*>(ui->fanModeComboBox->model());
        QModelIndex customModeIndex = model->index(2, ui->fanModeComboBox->modelColumn());
        QStandardItem *customMode = model->itemFromIndex(customModeIndex);
        customMode->setEnabled(false);
        customMode->setToolTip("To use this mode you must make a fan curve first");
    }

    if (settings.contains("voltageOffset")) {
        latestVoltOfs = settings.value("voltageOffset").toInt();

        ui->voltageSlider->setValue(latestVoltOfs);
        ui->voltageSpinBox->setValue(latestVoltOfs);
    }
    if (settings.contains("powerLimit")) {
        latestPowerLim = settings.value("powerLimit").toInt();

        ui->powerLimSlider->setValue(latestPowerLim);
        ui->powerLimSlider->setValue(latestPowerLim);
    }
    if (settings.contains("clockFrequencyOffset")) {
        latestClkOfs = settings.value("clockFrequencyOffset").toInt();

        ui->frequencySlider->setValue(latestClkOfs);
        ui->frequencySpinBox->setValue(latestClkOfs);
    }
    if (settings.contains("memoryClockOffset")) {
        latestMemClkOfs=settings.value("memoryClockOffset").toInt();

        ui->memClkSlider->setValue(latestMemClkOfs);
        ui->memClkSlider->setValue(latestMemClkOfs);
    }
    if (settings.contains("fanControlMode")) {
        fanControlMode = settings.value("fanControlMode").toInt();
        ui->fanModeComboBox->setCurrentIndex(fanControlMode);
    }
    // Check which GPU index corresponds to the UUID and set the combo box selection to it
    //ui->GPUComboBox->setCurrentIndex(UUIDList.indexOf(latestUUID));
    ui->statusBar->showMessage("Profile settings loaded.", 7000);
}

void MainWindow::on_newProfile_closed()
{
    // If currentProfile doesn't exist anymore the first profile will be the first entry
    QSettings settings("tuxclocker");
    QStringList groups = settings.childGroups();
    if (!groups.contains(currentProfile)) {
        for (int i=0; i<groups.size(); i++) {
            settings.beginGroup(groups[i]);
            if (settings.value("isProfile").toBool()) {
                settings.endGroup();
                currentProfile = groups[i];
                break;
            }
            settings.endGroup();
        }
    }
    //settings.endGroup();
    settings.setValue("General/currentProfile", currentProfile);
    ui->profileComboBox->clear();
    //ui->GPUComboBox->clear();
    checkForProfiles();
    loadProfileSettings();
    queryGPUs();
}
void MainWindow::saveProfileSettings()
{
    QSettings settings("tuxclocker");
    settings.beginGroup("General");
    settings.setValue("latestUUID", nv->GPUList[currentGPUIndex].uuid);
    settings.endGroup();
    settings.beginGroup(currentProfile);
    settings.beginGroup(nv->GPUList[currentGPUIndex].uuid);
    settings.setValue("voltageOffset", nv->GPUList[currentGPUIndex].voltageOffset);
    settings.setValue("powerLimit", latestPowerLim);
    settings.setValue("clockFrequencyOffset", latestClkOfs);
    settings.setValue("memoryClockOffset", latestMemClkOfs);
    settings.setValue("fanControlMode", fanControlMode);
}

void MainWindow::generateFanPoint()
{
    // Calculate the value for fan speed based on temperature
    // First check if the fan speed should be y[0] or y[final]
    int index = 0;
    if (temp <= xCurvePoints[0]) {
        targetFanSpeed = yCurvePoints[0];
    }
    if (temp >= xCurvePoints[xCurvePoints.size()-1]) {
        targetFanSpeed = yCurvePoints[yCurvePoints.size()-1];
    } else {
        // Get the index of the leftmost point of the interpolated interval by comparing it to temperature
        for (int i=0; i<xCurvePoints.size(); i++) {
            if (temp >= xCurvePoints[i] && temp <= xCurvePoints[i+1]) {
                index = i;
                break;
            }
        }
        // Check if the change in x is zero to avoid dividing by it
        if (xCurvePoints[index] - xCurvePoints[index + 1] == 0) {
            targetFanSpeed = yCurvePoints[index+1];
        } else {
            targetFanSpeed = (((yCurvePoints[index + 1] - yCurvePoints[index]) * (temp - xCurvePoints[index])) / (xCurvePoints[index + 1] - xCurvePoints[index])) + yCurvePoints[index];
        }
    }
    QProcess process;
    QString input = nvFanSpeedSet;
    input.append(QString::number(targetFanSpeed));
    process.start(input);
    process.waitForFinished(-1);
}
void MainWindow::on_frequencySlider_valueChanged(int value)
{
    // Sets the input field value to slider value
    QString freqText = QString::number(value);
    ui->frequencySpinBox->setValue(value);
}

void MainWindow::on_frequencySpinBox_valueChanged(int arg1)
{
    ui->frequencySlider->setValue(arg1);
}

void MainWindow::on_newProfile_clicked()
{
    newProfile *newprof = new newProfile(this);
    newprof->setAttribute(Qt::WA_DeleteOnClose);
    connect(newprof, SIGNAL(destroyed(QObject*)), SLOT(on_newProfile_closed()));
    newprof->setModal(true);
    newprof->exec();
}

void MainWindow::on_powerLimSlider_valueChanged(int value)
{
    ui->powerLimSpinBox->setValue(value);
}
void MainWindow::on_powerLimSpinBox_valueChanged(int arg1)
{
    ui->powerLimSlider->setValue(arg1);
}
void MainWindow::on_memClkSlider_valueChanged(int value)
{
    ui->memClkSpinBox->setValue(value);
}
void MainWindow::on_memClkSpinBox_valueChanged(int arg1)
{
    ui->memClkSlider->setValue(arg1);
}
void MainWindow::on_voltageSlider_valueChanged(int value)
{
    ui->voltageSpinBox->setValue(value);
}
void MainWindow::on_voltageSpinBox_valueChanged(int arg1)
{
    ui->voltageSlider->setValue(arg1);
}
void MainWindow::on_fanSlider_valueChanged(int value)
{
    ui->fanSpinBox->setValue(value);
    fanUpdaterDisablerTimer->start(5000);
    fanUpdaterDisablerTimer->setSingleShot(true);
    disconnect(fanUpdateTimer, SIGNAL(timeout()), this, SLOT(fanSpeedUpdater()));
    connect(fanUpdaterDisablerTimer, SIGNAL(timeout()), this, SLOT(enableFanUpdater()));
}
void MainWindow::on_fanSpinBox_valueChanged(int arg1)
{
    ui->fanSlider->setValue(arg1);
}
void MainWindow::enableFanUpdater()
{
    connect(fanUpdateTimer, SIGNAL(timeout()), this, SLOT(fanSpeedUpdater()));
}
void MainWindow::on_applyButton_clicked()
{
    QSettings settings("tuxclocker");
    settings.beginGroup("General");
    QString prevProfile = settings.value("currentProfile").toString();
    settings.setValue("currentProfile", currentProfile);


    applyGPUSettings();
    //saveProfileSettings();
    //applyFanMode();
    //setupMonitorTab();
}
void MainWindow::on_editFanCurveButton_pressed()
{
    editProfile *editProf = new editProfile(this);
    editProf->setAttribute(Qt::WA_DeleteOnClose);
    connect(editProf, SIGNAL(destroyed(QObject*)), SLOT(on_editProfile_closed()));
    editProf->setModal(true);
    editProf->exec();

}
void MainWindow::on_editProfile_closed()
{
    // Clear the existing curve points and load the new ones
    xCurvePoints.clear();
    yCurvePoints.clear();
    loadProfileSettings();
}

void MainWindow::on_fanModeComboBox_currentIndexChanged(int index)
{
    nv->GPUList[currentGPUIndex].fanControlMode = index;
}

void MainWindow::on_actionManage_profiles_triggered()
{
    newProfile *np = new newProfile(this);
    np->setAttribute(Qt::WA_DeleteOnClose);
    connect(np, SIGNAL(destroyed(QObject*)), SLOT(on_newProfile_closed()));
    np->setModal(false);
    np->exec();
}

/*void MainWindow::on_profileComboBox_currentTextChanged(const QString &arg1)
{

}*/

void MainWindow::on_GPUComboBox_currentIndexChanged(int index)
{
    currentGPUIndex = index;
    // Call the NVML setup function so the index of the device struct is updated
    nv->setupNVML(currentGPUIndex);
    nv->queryGPUPowerLimitLimits(currentGPUIndex);
    nv->queryGPUPowerLimit(currentGPUIndex);
    // Change the slider ranges and availabilities according to the GPU
    if (nv->GPUList[index].overClockAvailable) {
        ui->frequencySlider->setRange(nv->GPUList[index].minCoreClkOffset, nv->GPUList[index].maxCoreClkOffset);
        ui->memClkSlider->setRange(nv->GPUList[index].minMemClkOffset, nv->GPUList[index].maxMemClkOffset);
    } else {
        ui->frequencySlider->setEnabled(false);
        ui->frequencySpinBox->setEnabled(false);
        ui->memClkSlider->setEnabled(false);
        ui->memClkSpinBox->setEnabled(false);
    }

    if (nv->GPUList[index].overVoltAvailable) {
        ui->voltageSlider->setRange(nv->GPUList[index].minVoltageOffset, nv->GPUList[index].maxVoltageOffset);
    } else {
        ui->voltageSlider->setEnabled(false);
        ui->voltageSpinBox->setEnabled(false);
    }

    if (!nv->GPUList[currentGPUIndex].manualFanCtrlAvailable) {
        // If manual fan control is not available for the GPU, disable the option
        QStandardItemModel *model = qobject_cast<QStandardItemModel*>(ui->fanModeComboBox->model());
        QModelIndex manualModeIndex = model->index(1, ui->fanModeComboBox->modelColumn());
        QStandardItem *manualMode = model->itemFromIndex(manualModeIndex);
        manualMode->setEnabled(false);
        manualMode->setToolTip("Manual fan control is not available for current GPU");
    }
}
