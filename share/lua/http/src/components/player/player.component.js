Vue.component('player', {
    template: '#player-template',
    computed: {
        ...Vuex.mapState({
            status: state => state.status.data
        }),
    },
    methods: {
        fetchStatus() {
            this.$store.dispatch('status/fetchStatus');
        },
        refreshStatus() {
            if (this.interval) {
                clearTimeout(this.interval);
                this.interval = null;
            }
            this.fetchStatus();
            this.interval = setInterval(() => {
                this.fetchStatus();
            }, 1000);
        },
        setVideo(videoSrc, type) {
            this.player.source({
                type: 'video',
                title: '',
                sources: [{
                    src: videoSrc,
                    type
                }],
                poster: 'assets/vlc-icons/256x256/vlc.png'
            });
        },
        playItem(src, id) {
            setVideo(src, 'video/mp4', id);
            this.player.play();
        },
        plyrInit() {
            let player = plyr.setup({
                showPosterOnEnd: true
            });
            this.player = player[0];
            // setVideo('S6IP_6HG2QE','youtube');
            this.player.on('pause', () => {
                if (this.status && this.status.state !== 'paused') {
                    this.$store.dispatch('status/pause');
                }
            });

            this.player.on('play', () => {
                if (this.status && this.status.state !== 'playing') {
                    this.$store.dispatch('status/play', this.status.currentplid);
                }
            });

            this.player.on('volumechange', (event) => {
                const volume = event.detail.plyr.getVolume() * 255;
                this.$store.dispatch('status/updateVolume', volume);
            });
        },
        handleState(state) {
            if (this.previousState && this.previousState == state) {
                return;
            }
            this.previousState = state;
            switch (state) {
                case 'playing':
                    this.player.play();
                    break;
                case 'paused':
                    this.player.pause();
                    break;
                case 'stopped':
                    this.player.stop();
                    break;
            }
        }
    },
    mounted() {
        this.plyrInit();
        // Avoid Uncaught (in promise) DOMException: play() failed because the user didn't interact with the document first.
        document.body.addEventListener('mousemove', () => {
            this.refreshStatus();
        }, {
            once: true
        });
        this.$store.subscribe((mutation, payload) => {
            switch (mutation.type) {
                case 'status/setStatus':
                    this.handleState(payload.status.data.state);
                    break;
            }
        });
    }
});
