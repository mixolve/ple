import AVFAudio
import AudioToolbox
import SwiftUI

@MainActor
final class AudioHostModel: ObservableObject {
    @Published var isPlaying = false
    @Published var status = "loading auv3 effects..."
    @Published var components: [AVAudioUnitComponent] = []
    @Published var selectedComponent: AVAudioUnitComponent?

    private let engine = AVAudioEngine()
    private var sourceNode: AVAudioSourceNode?
    private var effectNode: AVAudioUnit?
    private var phase = 0.0
    private let eveManufacturer = AudioHostModel.ostype("YOYO")
    private let eveSubtype = AudioHostModel.ostype("eve1")

    func start() {
        configureSession()
        refreshComponents()
        buildGraph(effect: nil)
    }

    func refreshComponents() {
        let description = AudioComponentDescription(
            componentType: kAudioUnitType_Effect,
            componentSubType: 0,
            componentManufacturer: 0,
            componentFlags: 0,
            componentFlagsMask: 0
        )

        components = AVAudioUnitComponentManager.shared()
            .components(matching: description)
            .sorted { $0.name < $1.name }

        if selectedComponent == nil {
            selectedComponent = components.first {
                $0.audioComponentDescription.componentManufacturer == eveManufacturer
                    && $0.audioComponentDescription.componentSubType == eveSubtype
            }
        }

        status = components.isEmpty ? "no auv3 effects found" : "found \(components.count) auv3 effect(s)"
    }

    func select(_ component: AVAudioUnitComponent?) {
        selectedComponent = component
        loadSelectedEffect()
    }

    func togglePlayback() {
        if isPlaying {
            engine.pause()
            isPlaying = false
            status = "paused"
            return
        }

        if !engine.isRunning {
            do {
                try engine.start()
            } catch {
                status = "engine error: \(error.localizedDescription)"
                return
            }
        }

        isPlaying = true
        status = effectNode == nil ? "playing dry signal" : "playing through \(selectedComponent?.name.lowercased() ?? "effect")"
    }

    func loadSelectedEffect() {
        guard let selectedComponent else {
            buildGraph(effect: nil)
            return
        }

        status = "loading \(selectedComponent.name.lowercased())..."
        AVAudioUnit.instantiate(with: selectedComponent.audioComponentDescription, options: []) { [weak self] audioUnit, error in
            Task { @MainActor in
                guard let self else { return }
                if let error {
                    self.status = "plugin error: \(error.localizedDescription)"
                    self.buildGraph(effect: nil)
                    return
                }

                self.buildGraph(effect: audioUnit)
                self.status = "loaded \(selectedComponent.name.lowercased())"
            }
        }
    }

    private func buildGraph(effect: AVAudioUnit?) {
        let wasPlaying = isPlaying
        if engine.isRunning {
            engine.stop()
        }

        engine.reset()
        engine.attachedNodes.forEach { node in
            if node !== engine.mainMixerNode && node !== engine.outputNode {
                engine.detach(node)
            }
        }

        let format = AVAudioFormat(standardFormatWithSampleRate: 44_100, channels: 2)!
        let source = AVAudioSourceNode { [weak self] _, _, frameCount, audioBufferList -> OSStatus in
            guard let self else { return noErr }
            let abl = UnsafeMutableAudioBufferListPointer(audioBufferList)
            let frequency = 220.0
            let sampleRate = format.sampleRate
            let phaseIncrement = 2.0 * Double.pi * frequency / sampleRate

            for frame in 0..<Int(frameCount) {
                let sample = Float(sin(self.phase) * 0.18)
                self.phase += phaseIncrement
                if self.phase >= 2.0 * Double.pi {
                    self.phase -= 2.0 * Double.pi
                }

                for buffer in abl {
                    buffer.mData?.assumingMemoryBound(to: Float.self)[frame] = sample
                }
            }

            return noErr
        }

        sourceNode = source
        effectNode = effect

        engine.attach(source)
        if let effect {
            engine.attach(effect)
            engine.connect(source, to: effect, format: format)
            engine.connect(effect, to: engine.mainMixerNode, format: format)
        } else {
            engine.connect(source, to: engine.mainMixerNode, format: format)
        }

        engine.connect(engine.mainMixerNode, to: engine.outputNode, format: format)
        engine.prepare()

        if wasPlaying {
            do {
                try engine.start()
                isPlaying = true
            } catch {
                isPlaying = false
                status = "engine error: \(error.localizedDescription)"
            }
        }
    }

    private func configureSession() {
        do {
            let session = AVAudioSession.sharedInstance()
            try session.setCategory(.playback, mode: .default)
            try session.setActive(true)
        } catch {
            status = "audio session error: \(error.localizedDescription)"
        }
    }

    private static func ostype(_ string: String) -> OSType {
        string.utf8.reduce(0) { ($0 << 8) + OSType($1) }
    }
}

struct ContentView: View {
    @StateObject private var model = AudioHostModel()

    var body: some View {
        NavigationStack {
            VStack(alignment: .leading, spacing: 20) {
                VStack(alignment: .leading, spacing: 8) {
                    Text("ple")
                        .font(.largeTitle)
                        .fontWeight(.semibold)

                    Text(model.status)
                        .font(.callout)
                        .foregroundStyle(.secondary)
                }

                Button {
                    model.togglePlayback()
                } label: {
                    Label(model.isPlaying ? "pause" : "play", systemImage: model.isPlaying ? "pause.fill" : "play.fill")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.borderedProminent)
                .controlSize(.large)

                Picker("auv3 effect", selection: Binding(
                    get: { model.selectedComponent?.audioComponentDescription.componentSubType ?? 0 },
                    set: { subtype in
                        model.select(model.components.first { $0.audioComponentDescription.componentSubType == subtype })
                    }
                )) {
                    Text("dry").tag(0 as OSType)
                    ForEach(model.components, id: \.audioComponentDescription.componentSubType) { component in
                        Text(component.name.lowercased()).tag(component.audioComponentDescription.componentSubType)
                    }
                }
                .pickerStyle(.navigationLink)

                Button("refresh plugins") {
                    model.refreshComponents()
                    model.loadSelectedEffect()
                }
                .buttonStyle(.bordered)

                Spacer()
            }
            .padding()
            .navigationTitle("host")
        }
        .tint(Color(red: 0.6, green: 0.6, blue: 1.0))
        .task {
            model.start()
            model.loadSelectedEffect()
        }
    }
}

#Preview {
    ContentView()
}
