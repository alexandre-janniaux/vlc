Vue.component('sidenav', {
    template: '#sidenav-template',
    methods: {
        openNav() {
            if (window.screen.width <= 480) {
                $('#sideNav').width('60%');
            } else {
                $('#sideNav').width('20%');
            }
        },
        closeNav() {
            $('#sideNav').width('0');
        }
    },
    created() {
        this.$store.subscribeAction((mutation) => {
            switch (mutation.type) {
                case 'layout/openNavbar':
                    this.openNav();
                    break;
                case 'layout/closeNavbar':
                    this.closeNav();
                    break;
            }
        });
    }
});
