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

package org.tensorflow;

import java.util.Iterator;

/**
 * A data flow graph representing a TensorFlow computation.
 *
 * <p>Instances of a Graph are thread-safe.
 *
 * <p><b>WARNING:</b> Resources consumed by the Graph object must be explicitly freed by invoking
 * the {@link #close()} method then the Graph object is no longer needed.
 */
public final class Graph implements AutoCloseable {

  /** Create an empty Graph. */
  public Graph() {
    nativeHandle = allocate();
  }

  /** Create a Graph from an existing handle (takes ownership). */
  Graph(long nativeHandle) {
    this.nativeHandle = nativeHandle;
  }

  /**
   * Release resources associated with the Graph.
   *
   * <p>Blocks until there are no active {@link Session} instances referring to this Graph. A Graph
   * is not usable after close returns.
   */
  @Override
  public void close() {
    synchronized (nativeHandleLock) {
      if (nativeHandle == 0) {
        return;
      }
      while (refcount > 0) {
        try {
          nativeHandleLock.wait();
        } catch (InterruptedException e) {
          Thread.currentThread().interrupt();
          // Possible leak of the graph in this case?
          return;
        }
      }
      delete(nativeHandle);
      nativeHandle = 0;
    }
  }

  /**
   * Returns the operation (node in the Graph) with the provided name.
   *
   * <p>Or {@code null} if no such operation exists in the Graph.
   */
  public Operation operation(String name) {
    synchronized (nativeHandleLock) {
      long oph = operation(nativeHandle, name);
      if (oph == 0) {
        return null;
      }
      return new Operation(this, oph);
    }
  }

  /**
   * Iterator over all the {@link Operation}s in the graph.
   *
   * <p>The order of iteration is unspecified. Consumers of the iterator will receive no
   * notification should the underlying graph change during iteration.
   */
  public Iterator<Operation> operations() {
    return new OperationIterator(this);
  }

  /**
   * Returns a builder to add {@link Operation}s to the Graph.
   *
   * @param type of the Operation (i.e., identifies the computation to be performed)
   * @param name to refer to the created Operation in the graph.
   * @return an {@link OperationBuilder}, which will add the Operation to the graph when {@link
   *     OperationBuilder#build()} is invoked. If {@link OperationBuilder#build()} is not invoked,
   *     then some resources may leak.
   */
  public OperationBuilder opBuilder(String type, String name) {
    return new OperationBuilder(this, type, name);
  }

  /**
   * Import a serialized representation of a TensorFlow graph.
   *
   * <p>The serialized representation of the graph, often referred to as a <i>GraphDef</i>, can be
   * generated by {@link #toGraphDef()} and equivalents in other language APIs.
   *
   * @throws IllegalArgumentException if graphDef is not a recognized serialization of a graph.
   * @see #importGraphDef(byte[], String)
   */
  public void importGraphDef(byte[] graphDef) throws IllegalArgumentException {
    importGraphDef(graphDef, "");
  }

  /**
   * Import a serialized representation of a TensorFlow graph.
   *
   * @param graphDef the serialized representation of a TensorFlow graph.
   * @param prefix a prefix that will be prepended to names in graphDef
   * @throws IllegalArgumentException if graphDef is not a recognized serialization of a graph.
   * @see #importGraphDef(byte[])
   */
  public void importGraphDef(byte[] graphDef, String prefix) throws IllegalArgumentException {
    if (graphDef == null || prefix == null) {
      throw new IllegalArgumentException("graphDef and prefix cannot be null");
    }
    synchronized (nativeHandleLock) {
      importGraphDef(nativeHandle, graphDef, prefix);
    }
  }

  /**
   * Generate a serialized representation of the Graph.
   *
   * @see #importGraphDef(byte[])
   * @see #importGraphDef(byte[], String)
   */
  public byte[] toGraphDef() {
    synchronized (nativeHandleLock) {
      return toGraphDef(nativeHandle);
    }
  }

  /**
   * Adds operations to compute the partial derivatives of sum of {@code y}s w.r.t {@code x}s,
   * i.e., {@code d(y_1 + y_2 + ...)/dx_1, d(y_1 + y_2 + ...)/dx_2...}
   * <p> 
   * {@code dx} are used as initial gradients (which represent the symbolic partial derivatives of some loss function 
   * {@code L} w.r.t. {@code y}). {@code dx} must be null or have size of {@code y}.
   * <p>
   * If {@code dx} is null, the implementation will use dx of {@link org.tensorflow.op.core.OnesLike OnesLike} for all
   * shapes in {@code y}.
   * <p>
   * {@code prefix} is used as the name prefix applied to all nodes added to the graph to compute gradients. It must
   * be unique within the provided graph or the operation will fail. 
   * <p>
   * If {@code prefix} is null, then one will be chosen automatically.
   * 
   * @param prefix unique string prefix applied before the names of nodes added to the graph to compute gradients.
   *               If null, a default one will be chosen.
   * @param y output of the function to derive
   * @param x inputs of the function for which partial derivatives are computed
   * @param dx if not null, the partial derivatives of some loss function {@code L} w.r.t. {@code y}
   * @return the partial derivatives {@code dy} with the size of {@code x}
   */
  public Output<?>[] addGradients(String prefix, Output<?>[] y, Output<?>[] x, Output<?>[] dx) {
    Output<?>[] dy = new Output<?>[x.length];
    final long[] yHandles = new long[y.length];
    final int[] yIndices = new int[y.length];
    final long[] xHandles = new long[x.length];
    final int[] xIndices = new int[x.length];
    long[] dxHandles = null;
    int[] dxIndices = null;

    try (Reference ref = ref()) {
      for (int i = 0; i < y.length; ++i) {
        yHandles[i] = y[i].op().getUnsafeNativeHandle();
        yIndices[i] = y[i].index();
      }
      for (int i = 0; i < x.length; ++i) {
        xHandles[i] = x[i].op().getUnsafeNativeHandle();
        xIndices[i] = x[i].index();
      }
      if (dx != null && dx.length > 0) {
        dxHandles = new long[dx.length];
        dxIndices = new int[dx.length];

        for (int i = 0; i < dx.length; ++i) {
          dxHandles[i] = dx[i].op().getUnsafeNativeHandle();
          dxIndices[i] = dx[i].index();
        }
      }
      // Gradient outputs are returned in two continuous arrays concatenated into one. The first holds the native 
      // handles of the gradient operations while the second holds the index of their output e.g. given 
      // xHandles = [x0Handle, x1Handle, ...] and xIndices = [x0Index, x1Index, ..], we obtain 
      // dy = [dy0Handle, dy1Handle, ..., dy0Index, dy1Index, ...]
      long[] dyHandlesAndIndices =
            addGradients(ref.nativeHandle(), prefix, yHandles, yIndices, xHandles, xIndices, dxHandles, dxIndices);
      int ndy = dyHandlesAndIndices.length >> 1;
      if (ndy != dy.length) {
        throw new IllegalStateException(String.valueOf(ndy) + " gradients were added to the graph when " + dy.length
            + " were expected");
      }
      for (int i = 0, j = ndy; i < ndy; ++i, ++j) {
        Operation op = new Operation(this, dyHandlesAndIndices[i]);
        dy[i] = new Output<>(op, (int) dyHandlesAndIndices[j]);
      }
    }
    return dy;
  }

  /**
   * Adds operations to compute the partial derivatives of sum of {@code y}s w.r.t {@code x}s,
   * i.e., {@code dy/dx_1, dy/dx_2...}
   * <p> 
   * This is a simplified version of {@link #addGradients(Output[], Output[], Output[]) where {@code y} is
   * a single output, {@code dx} is null and {@code prefix} is null.
   * 
   * @param y output of the function to derive
   * @param x inputs of the function for which partial derivatives are computed
   * @return the partial derivatives {@code dy} with the size of {@code x}
   */
  public Output<?>[] addGradients(Output<?> y, Output<?>[] x) {
    return addGradients(null, new Output<?>[]{y}, x, null);
  }
  
  private final Object nativeHandleLock = new Object();
  private long nativeHandle;
  private int refcount = 0;

  // Related native objects (such as the TF_Operation object backing an Operation instance)
  // have a validity tied to that of the Graph. The handles to those native objects are not
  // valid after Graph.close() has been invoked.
  //
  // Instances of the Reference class should be used to ensure the Graph has not been closed
  // while dependent handles are in use.
  class Reference implements AutoCloseable {
    private Reference() {
      synchronized (Graph.this.nativeHandleLock) {
        active = Graph.this.nativeHandle != 0;
        if (!active) {
          throw new IllegalStateException("close() has been called on the Graph");
        }
        active = true;
        Graph.this.refcount++;
      }
    }

    @Override
    public void close() {
      synchronized (Graph.this.nativeHandleLock) {
        if (!active) {
          return;
        }
        active = false;
        if (--Graph.this.refcount == 0) {
          Graph.this.nativeHandleLock.notifyAll();
        }
      }
    }

    public long nativeHandle() {
      synchronized (Graph.this.nativeHandleLock) {
        return active ? Graph.this.nativeHandle : 0;
      }
    }

    private boolean active;
  }

  Reference ref() {
    return new Reference();
  }

  private static final class OperationIterator implements Iterator<Operation> {

    OperationIterator(Graph g) {
      this.graph = g;
      this.operation = null;
      this.position = 0;
      this.advance();
    }

    private final void advance() {
      Graph.Reference reference = this.graph.ref();

      this.operation = null;

      try {
        long[] nativeReturn = nextOperation(reference.nativeHandle(), this.position);

        if ((nativeReturn != null) && (nativeReturn[0] != 0)) {
          this.operation = new Operation(this.graph, nativeReturn[0]);
          this.position = (int) nativeReturn[1];
        }
      } finally {
        reference.close();
      }
    }

    @Override
    public boolean hasNext() {
      return (this.operation != null);
    }

    @Override
    public Operation next() {
      Operation rhett = this.operation;
      this.advance();
      return rhett;
    }

    @Override
    public void remove() {
      throw new UnsupportedOperationException("remove() is unsupported.");
    }

    private final Graph graph;
    private Operation operation;
    private int position;
  }

  private static native long allocate();

  private static native void delete(long handle);

  private static native long operation(long handle, String name);

  // This method returns the Operation native handle at index 0 and the new value for pos at index 1
  // (see TF_GraphNextOperation)
  private static native long[] nextOperation(long handle, int position);

  private static native void importGraphDef(long handle, byte[] graphDef, String prefix)
      throws IllegalArgumentException;

  private static native byte[] toGraphDef(long handle);

  private static native long[] addGradients(long handle, String prefix, long[] inputHandles, int[] inputIndices,
      long[] outputHandles, int[] outputIndices, long[] gradInputHandles, int[] gradInputIndices);

  static {
    TensorFlow.init();
  }
}
