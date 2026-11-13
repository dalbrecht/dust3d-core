import Dust3DAudioWav

enum Failure: Error { case encoding, copy, transfer }
actor Receiver {
    func receive(_ bytes: [UInt8]) -> Bool {
        bytes.count == 54 && Array(bytes[44...]) == [0, 128, 255, 255, 0, 0, 1, 0, 255, 127]
    }
}
@main struct Smoke {
    static func main() async throws {
        let samples: [Int16] = [-32768, -1, 0, 1, 32767]
        let copied: [UInt8] = try samples.withUnsafeBufferPointer { input in
            var status = DUST3D_AUDIO_WAV_ENCODING_FAILED
            guard let owner = dust3d_audio_wav_create(input.baseAddress, UInt64(input.count), 44100, 1, 54, &status),
                status == DUST3D_AUDIO_WAV_OK
            else { throw Failure.encoding }
            defer { dust3d_audio_wav_destroy(owner) }
            guard let source = dust3d_audio_wav_bytes(owner), dust3d_audio_wav_size(owner) == 54 else {
                throw Failure.copy
            }
            return Array(UnsafeBufferPointer(start: source, count: 54))
        }
        guard await Receiver().receive(copied) else { throw Failure.transfer }
        print("Swift 6 owned PCM/WAV copy and actor transfer passed")
    }
}
