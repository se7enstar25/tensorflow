/* Copyright 2016 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

import {updateWarningMessage} from './async';
import {ColorOption, DataSet, Projection, State} from './data';
import {DataProvider, getDataProvider, MetadataResult} from './data-loader';
import {HoverContext, HoverListener} from './hoverContext';
import * as knn from './knn';
import {Mode, ScatterPlot} from './scatterPlot';
import {ScatterPlotVisualizer3DLabels} from './scatterPlotVisualizer3DLabels';
import {ScatterPlotVisualizerCanvasLabels} from './scatterPlotVisualizerCanvasLabels';
import {ScatterPlotVisualizerSprites} from './scatterPlotVisualizerSprites';
import {ScatterPlotVisualizerTraces} from './scatterPlotVisualizerTraces';
import {SelectionChangedListener, SelectionContext} from './selectionContext';
import {BookmarkPanel} from './vz-projector-bookmark-panel';
import {DataPanel} from './vz-projector-data-panel';
import {InspectorPanel} from './vz-projector-inspector-panel';
import {MetadataCard} from './vz-projector-metadata-card';
import {ProjectionsPanel} from './vz-projector-projections-panel';
// tslint:disable-next-line:no-unused-variable
import {PolymerElement, PolymerHTMLElement} from './vz-projector-util';

const POINT_COLOR_UNSELECTED = 0x888888;
const POINT_COLOR_NO_SELECTION = 0x7575D9;
const POINT_COLOR_SELECTED = 0xFA6666;
const POINT_COLOR_HOVER = 0x760B4F;
const POINT_COLOR_MISSING = 'black';

/**
 * The minimum number of dimensions the data should have to automatically
 * decide to normalize the data.
 */
const THRESHOLD_DIM_NORMALIZE = 50;

export let ProjectorPolymer = PolymerElement({
  is: 'vz-projector',
  properties: {
    // Private.
    routePrefix: String,
    labelOption: {type: String, observer: '_labelOptionChanged'},
    colorOption: {type: Object, observer: '_colorOptionChanged'}
  }
});

export class Projector extends ProjectorPolymer implements SelectionContext,
                                                           HoverContext {
  // The working subset of the data source's original data set.
  currentDataSet: DataSet;

  private selectionChangedListeners: SelectionChangedListener[];
  private hoverListeners: HoverListener[];

  private dataSet: DataSet;
  private dom: d3.Selection<any>;
  private scatterPlot: ScatterPlot;
  private dim: number;

  private selectedPointIndices: number[];
  private neighborsOfFirstPoint: knn.NearestEntry[];
  private hoverPointIndex: number;

  private dataProvider: DataProvider;
  private inspectorPanel: InspectorPanel;

  private colorOption: ColorOption;
  private labelOption: string;
  private routePrefix: string;
  private normalizeData: boolean;
  private selectedProjection: Projection;

  /** Polymer component panels */
  private dataPanel: DataPanel;
  private bookmarkPanel: BookmarkPanel;
  private projectionsPanel: ProjectionsPanel;
  private metadataCard: MetadataCard;

  ready() {
    this.selectionChangedListeners = [];
    this.hoverListeners = [];
    this.selectedPointIndices = [];
    this.neighborsOfFirstPoint = [];

    this.dom = d3.select(this);
    this.dataPanel = this.$['data-panel'] as DataPanel;
    this.inspectorPanel = this.$['inspector-panel'] as InspectorPanel;
    this.inspectorPanel.initialize(this);
    this.bookmarkPanel = this.$['bookmark-panel'] as BookmarkPanel;
    this.bookmarkPanel.initialize(this);
    this.projectionsPanel = this.$['projections-panel'] as ProjectionsPanel;
    this.projectionsPanel.initialize(this);
    this.metadataCard = this.$['metadata-card'] as MetadataCard;

    getDataProvider(this.routePrefix, dataProvider => {
      this.dataProvider = dataProvider;
      this.dataPanel.initialize(this, dataProvider);
    });

    this.setupUIControls();
  }

  _labelOptionChanged() {
    let labelAccessor = (i: number): string => {
      return this.currentDataSet.points[i].metadata[this.labelOption] as string;
    };
    this.scatterPlot.setLabelAccessor(labelAccessor);
    this.metadataCard.setLabelOption(this.labelOption);
  }

  _colorOptionChanged() {
    const colors = this.recomputeScatterPlotColors(
        this.getLegendPointColorer(this.colorOption), this.selectedPointIndices,
        this.neighborsOfFirstPoint, this.hoverPointIndex);
    this.scatterPlot.setPointColors(colors);
    this.scatterPlot.render();
  }

  setNormalizeData(normalizeData: boolean) {
    this.normalizeData = normalizeData;
    this.setCurrentDataSet(this.dataSet.getSubset());
  }

  updateDataSet(ds: DataSet) {
    this.dataSet = ds;
    if (this.scatterPlot == null || this.dataSet == null) {
      // We are not ready yet.
      return;
    }
    this.normalizeData = this.dataSet.dim[1] >= THRESHOLD_DIM_NORMALIZE;
    this.dataPanel.setNormalizeData(this.normalizeData);
    this.setCurrentDataSet(this.dataSet.getSubset());
    this.inspectorPanel.datasetChanged();

    // Set the container to a fixed height, otherwise in Colab the
    // height can grow indefinitely.
    let container = this.dom.select('#container');
    container.style('height', container.property('clientHeight') + 'px');
  }

  /**
   * Registers a listener to be called any time the selected point set changes.
   */
  registerSelectionChangedListener(listener: SelectionChangedListener) {
    this.selectionChangedListeners.push(listener);
  }

  filterDataset() {
    this.setCurrentDataSet(
        this.currentDataSet.getSubset(this.selectedPointIndices));
    this.clearSelection();
    this.scatterPlot.recreateScene();
  }

  resetFilterDataset() {
    this.setCurrentDataSet(this.dataSet.getSubset(null));
    this.selectedPointIndices = [];
  }

  /**
   * Used by clients to indicate that a selection has occurred.
   */
  notifySelectionChanged(newSelectedPointIndices: number[]) {
    this.selectedPointIndices = newSelectedPointIndices;
    let neighbors: knn.NearestEntry[] = [];

    if (newSelectedPointIndices.length === 1) {
      neighbors = this.currentDataSet.findNeighbors(
          newSelectedPointIndices[0], this.inspectorPanel.distFunc,
          this.inspectorPanel.numNN);
      this.metadataCard.updateMetadata(
          this.dataSet.points[newSelectedPointIndices[0]].metadata);
    } else {
      this.metadataCard.updateMetadata(null);
    }

    this.selectionChangedListeners.forEach(
        l => l(this.selectedPointIndices, neighbors));
  }

  mergeMetadata(result: MetadataResult): void {
    let numTensors = this.dataSet.points.length;
    if (result.metadata.length !== numTensors) {
      updateWarningMessage(
          `Number of tensors (${numTensors}) do not match` +
          ` the number of lines in metadata (${result.metadata.length}).`);
    }
    this.dataSet.mergeMetadata(result.metadata);
    this.setCurrentDataSet(this.dataSet.getSubset());
    this.dataSet.spriteImage = result.spriteImage;
    this.dataSet.metadata = result.datasetMetadata;
    this.inspectorPanel.metadataChanged(result);
    this.projectionsPanel.metadataChanged(result);
  }

  /**
   * Registers a listener to be called any time the mouse hovers over a point.
   */
  registerHoverListener(listener: HoverListener) {
    this.hoverListeners.push(listener);
  }

  /**
   * Used by clients to indicate that a hover is occurring.
   */
  notifyHoverOverPoint(pointIndex: number) {
    this.hoverListeners.forEach(l => l(pointIndex));
  }

  private getLegendPointColorer(colorOption: ColorOption):
      (index: number) => string {
    if ((colorOption == null) || (colorOption.map == null)) {
      return null;
    }
    const colorer = (i: number) => {
      let value = this.currentDataSet.points[i].metadata[this.colorOption.name];
      if (value == null) {
        return POINT_COLOR_MISSING;
      }
      return colorOption.map(value);
    };
    return colorer;
  }

  private recomputeScatterPlotColors(
      legendPointColorer: (index: number) => string,
      selectedPointIndices: number[], neighborsOfFirstPoint: knn.NearestEntry[],
      hoverPointIndex: number): Float32Array {
    if (this.currentDataSet == null) {
      return null;
    }

    const colors = new Float32Array(this.currentDataSet.points.length * 3);

    // Give all points the unselected color.
    {
      const n = this.currentDataSet.points.length;
      let dst = 0;
      if (selectedPointIndices.length > 0) {
        const c = new THREE.Color(POINT_COLOR_UNSELECTED);
        for (let i = 0; i < n; ++i) {
          colors[dst++] = c.r;
          colors[dst++] = c.g;
          colors[dst++] = c.b;
        }
      } else {
        if (legendPointColorer) {
          for (let i = 0; i < n; ++i) {
            const c = new THREE.Color(legendPointColorer(i));
            colors[dst++] = c.r;
            colors[dst++] = c.g;
            colors[dst++] = c.b;
          }
        } else {
          const c = new THREE.Color(POINT_COLOR_NO_SELECTION);
          for (let i = 0; i < n; ++i) {
            colors[dst++] = c.r;
            colors[dst++] = c.g;
            colors[dst++] = c.b;
          }
        }
      }
    }

    // Color the selected points.
    {
      const n = selectedPointIndices.length;
      const c = new THREE.Color(POINT_COLOR_SELECTED);
      for (let i = 0; i < n; ++i) {
        let dst = selectedPointIndices[i] * 3;
        colors[dst++] = c.r;
        colors[dst++] = c.g;
        colors[dst++] = c.b;
      }
    }

    // Color the neighbors.
    {
      const n = neighborsOfFirstPoint.length;
      const c = new THREE.Color(POINT_COLOR_SELECTED);
      for (let i = 0; i < n; ++i) {
        let dst = neighborsOfFirstPoint[i].index * 3;
        colors[dst++] = c.r;
        colors[dst++] = c.g;
        colors[dst++] = c.b;
      }
    }

    // Color the hover point.
    if (hoverPointIndex) {
      const c = new THREE.Color(POINT_COLOR_HOVER);
      let dst = hoverPointIndex * 3;
      colors[dst++] = c.r;
      colors[dst++] = c.g;
      colors[dst++] = c.b;
    }

    return colors;
  }

  clearSelection() {
    this.notifySelectionChanged([]);
    this.scatterPlot.setMode(Mode.HOVER);
  }

  private unsetCurrentDataSet() {
    this.currentDataSet.stopTSNE();
  }

  private setCurrentDataSet(ds: DataSet) {
    this.clearSelection();
    if (this.currentDataSet != null) {
      this.unsetCurrentDataSet();
    }
    this.currentDataSet = ds;
    if (this.normalizeData) {
      this.currentDataSet.normalize();
    }
    this.scatterPlot.setDataSet(this.currentDataSet, this.dataSet.spriteImage);
    this.dim = this.currentDataSet.dim[1];
    this.dom.select('span.numDataPoints').text(this.currentDataSet.dim[0]);
    this.dom.select('span.dim').text(this.currentDataSet.dim[1]);

    this.projectionsPanel.dataSetUpdated(this.currentDataSet, this.dim);
    this.showTab('pca');
  }

  private setupUIControls() {
    let self = this;

    // Global tabs
    this.dom.selectAll('.ink-tab').on('click', function() {
      let id = this.getAttribute('data-tab');
      self.showTab(id);
    });

    // View controls
    this.querySelector('#reset-zoom').addEventListener('click', () => {
      this.scatterPlot.resetZoom();
    });
    this.querySelector('#zoom-in').addEventListener('click', () => {
      this.scatterPlot.zoomStep(2);
    });
    this.querySelector('#zoom-out').addEventListener('click', () => {
      this.scatterPlot.zoomStep(0.5);
    });

    let selectModeButton = this.querySelector('#selectMode');
    selectModeButton.addEventListener('click', (event) => {
      this.scatterPlot.setMode(
          (selectModeButton as any).active ? Mode.SELECT : Mode.HOVER);
    });
    let nightModeButton = this.querySelector('#nightDayMode');
    nightModeButton.addEventListener('click', () => {
      this.scatterPlot.setDayNightMode((nightModeButton as any).active);
    });

    let labels3DModeButton = this.querySelector('#labels3DMode');
    labels3DModeButton.addEventListener('click', () => {
      this.createVisualizers((labels3DModeButton as any).active);
      this.scatterPlot.recreateScene();
      this.scatterPlot.update();
    });

    window.addEventListener('resize', () => {
      this.scatterPlot.resize();
    });

    this.scatterPlot = new ScatterPlot(
        this.getScatterContainer(),
        i => '' + this.currentDataSet.points[i].metadata[this.labelOption],
        this, this);
    this.createVisualizers(false);

    this.scatterPlot.onCameraMove(
        (cameraPosition: THREE.Vector3, cameraTarget: THREE.Vector3) =>
            this.bookmarkPanel.clearStateSelection());

    this.registerHoverListener(
        (hoverIndex: number) => this.onHover(hoverIndex));

    this.registerSelectionChangedListener(
        (selectedPointIndices: number[],
         neighborsOfFirstPoint: knn.NearestEntry[]) =>
            this.onSelectionChanged(
                selectedPointIndices, neighborsOfFirstPoint));
  }

  private onHover(hoverIndex: number) {
    this.hoverPointIndex = hoverIndex;
    let hoverText: string = '';
    if (hoverIndex != null) {
      const point = this.currentDataSet.points[hoverIndex];
      if (point.metadata[this.labelOption]) {
        hoverText = point.metadata[this.labelOption].toString();
      }
    }
    this.dom.select('#hoverInfo').text(hoverText);
    const colors = this.recomputeScatterPlotColors(
        this.getLegendPointColorer(this.colorOption), this.selectedPointIndices,
        this.neighborsOfFirstPoint, hoverIndex);
    this.scatterPlot.setPointColors(colors);
    this.scatterPlot.render();
  }

  private getScatterContainer(): d3.Selection<any> {
    return this.dom.select('#scatter');
  }

  private createVisualizers(inLabels3DMode: boolean) {
    const scatterPlot = this.scatterPlot;
    const selectionContext = this;
    const hoverContext = this;
    scatterPlot.removeAllVisualizers();

    if (inLabels3DMode) {
      scatterPlot.addVisualizer(
          new ScatterPlotVisualizer3DLabels(selectionContext, hoverContext));
    } else {
      scatterPlot.addVisualizer(
          new ScatterPlotVisualizerSprites(selectionContext, hoverContext));

      scatterPlot.addVisualizer(
          new ScatterPlotVisualizerTraces(selectionContext));

      scatterPlot.addVisualizer(new ScatterPlotVisualizerCanvasLabels(
          this.getScatterContainer(), selectionContext, hoverContext));
    }
  }

  private onSelectionChanged(
      selectedPointIndices: number[],
      neighborsOfFirstPoint: knn.NearestEntry[]) {
    this.selectedPointIndices = selectedPointIndices;
    this.neighborsOfFirstPoint = neighborsOfFirstPoint;
    this.dom.select('#hoverInfo')
        .text(`Selected ${selectedPointIndices.length} points`);
    this.inspectorPanel.updateInspectorPane(
        selectedPointIndices, neighborsOfFirstPoint);
    if (neighborsOfFirstPoint.length > 0) {
      this.showTab('inspector');
    }
    const colors = this.recomputeScatterPlotColors(
        this.getLegendPointColorer(this.colorOption), selectedPointIndices,
        neighborsOfFirstPoint, this.hoverPointIndex);
    this.scatterPlot.setPointColors(colors);
    this.scatterPlot.render();
  }

  public showTab(id: string) {
    let tab = this.dom.select('.ink-tab[data-tab="' + id + '"]');
    let pane =
        d3.select((tab.node() as HTMLElement).parentNode.parentNode.parentNode);
    pane.selectAll('.ink-tab').classed('active', false);
    tab.classed('active', true);
    pane.selectAll('.ink-panel-content').classed('active', false);
    pane.select('.ink-panel-content[data-panel="' + id + '"]')
        .classed('active', true);

    if (['pca', 'tsne', 'custom'].indexOf(id) !== -1) {
      this.projectionsPanel.showProjectionTab(id as Projection);
    }
  }

  setProjection(
      projection: Projection, xAccessor: (index: number) => number,
      yAccessor: (index: number) => number,
      zAccessor: (index: number) => number, xAxisLabel: string,
      yAxisLabel: string, deferUpdate = false) {
    this.selectedProjection = projection;
    this.scatterPlot.showTickLabels(false);
    this.scatterPlot.setPointAccessors(xAccessor, yAccessor, zAccessor);
    this.scatterPlot.setAxisLabels(xAxisLabel, yAxisLabel);
    if (!deferUpdate) {
      this.scatterPlot.update();
    }
    // Don't animate if we've defered updating as expensive computation is
    // happening to compute the projections, and there's no reason to animate
    // around non-existence projections.
    this.scatterPlot.recreateScene(!deferUpdate /** animate */);
  }

  notifyProjectionsUpdated() {
    this.scatterPlot.update();
  }

  /**
   * Gets the current view of the embedding and saves it as a State object.
   */
  getCurrentState(): State {
    let state: State = {};

    // Save the individual datapoint projections.
    state.projections = [];
    for (let i = 0; i < this.currentDataSet.points.length; i++) {
      state.projections.push(this.currentDataSet.points[i].projections);
    }

    state.selectedProjection = this.selectedProjection;
    state.selectedPoints = this.selectedPointIndices;
    state.cameraPosition = this.scatterPlot.getCameraPosition();
    state.cameraTarget = this.scatterPlot.getCameraTarget();

    return state;
  }

  /** Loads a State object into the world. */
  loadState(state: State) {
    for (let i = 0; i < state.projections.length; i++) {
      this.currentDataSet.points[i].projections = state.projections[i];
    }
    this.showTab(state.selectedProjection);
    if (state.selectedPoints.length > 0) {
      this.notifySelectionChanged(state.selectedPoints);
    }
    this.scatterPlot.setCameraPositionAndTarget(
        state.cameraPosition, state.cameraTarget);
  }
}

document.registerElement(Projector.prototype.is, Projector);
