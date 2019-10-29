Vue.component('sidenav', {
    template: '#sidenav-template',
    methods: {
        openNav() {
            $('#sideNav').addClass('opened');
            if (window.screen.width <= 480) {
                $('#sideNav').width('60%');
            } else {
                $('#sideNav').width('20%');
            }
        },
        closeNav() {
            $('#sideNav').removeClass('opened');
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
