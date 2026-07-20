import { endpoints } from './client'

export interface StreamEnvelope {
  topic: string
  payload: ArrayBuffer
  receivedAt: number
}

// Binary frames are intentionally kept as ArrayBuffer. A generated Protobuf
// decoder can be added here without changing components or connection code.
export function connectMarketStream(onMessage: (message: StreamEnvelope) => void) {
  const socket = new WebSocket(endpoints.stream)
  socket.binaryType = 'arraybuffer'
  socket.onmessage = (event) => {
    if (event.data instanceof ArrayBuffer) {
      onMessage({ topic: 'market.binary', payload: event.data, receivedAt: performance.now() })
    }
  }
  return socket
}
